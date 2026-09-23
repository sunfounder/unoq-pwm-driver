/*
    UNOQ_PWMServoDriver - implementation.

    Two engines cooperate behind one API:

      * Hardware engine - pins listed in the board devicetree `pwms` property are
        driven with pwm_set_dt(), giving an exact frequency and a 1 us pulse
        resolution with no CPU cost.

      * Software engine - every other GPIO is driven from a single Zephyr
        k_timer used in one-shot mode. All channels rise together at the start
        of the period, then the timer is re-armed once per distinct pulse width
        to drop the matching pins low again. Granularity is one kernel tick
        (100 us on the UNO Q, i.e. CONFIG_SYS_CLOCK_TICKS_PER_SEC == 10000).

    SPDX-License-Identifier: MPL-2.0
*/

#include "UNOQ_PWMServoDriver.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* ------------------------------------------------------------------------- */
/* Software PWM engine state.                                                */
/* ------------------------------------------------------------------------- */
namespace {

struct SwChannel {
	bool active;
	uint32_t high_us; /* Time spent HIGH, measured from the period start. */
};

SwChannel sw_chan[UNOQ_PWM_PIN_COUNT];

/* Sorted, de-duplicated list of instants at which the engine has work to do,
 * terminated by one entry equal to sw_period_us marking the period boundary. */
uint32_t sw_slot[UNOQ_PWM_PIN_COUNT + 1];
uint16_t sw_slot_count = 0;
uint16_t sw_slot_idx = 0;
uint16_t sw_active_count = 0;
uint32_t sw_period_us = 20000;
uint32_t sw_now_us = 0;
bool sw_running = false;

struct k_timer sw_timer;
bool sw_timer_ready = false;

void sw_arm_next();

/** @brief Raise every channel that is neither permanently low nor permanently high. */
void sw_raise_all() {
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (sw_chan[i].active && sw_chan[i].high_us > 0 && sw_chan[i].high_us < sw_period_us) {
			(void)gpio_pin_set_dt(&unoq_gpio[i], 1);
		}
	}
}

/** @brief Advance the engine to the slot it was armed for, then re-arm. */
void sw_service(void) {
	sw_now_us = sw_slot[sw_slot_idx];

	if (sw_now_us >= sw_period_us) {
		/* Period boundary: start the next frame. */
		sw_now_us = 0;
		sw_slot_idx = 0;
		sw_raise_all();
		sw_arm_next();
		return;
	}

	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (sw_chan[i].active && sw_chan[i].high_us == sw_now_us) {
			(void)gpio_pin_set_dt(&unoq_gpio[i], 0);
		}
	}
	sw_slot_idx++;
	sw_arm_next();
}

void sw_arm_next() {
	if (!sw_running || sw_slot_idx >= sw_slot_count) {
		return;
	}
	const uint32_t next = sw_slot[sw_slot_idx];
	if (next <= sw_now_us) {
		/* Already reached: never re-arm with a zero or negative delta. */
		return;
	}
	k_timer_start(&sw_timer, K_USEC(next - sw_now_us), K_NO_WAIT);
}

void sw_expiry(struct k_timer *timer) {
	ARG_UNUSED(timer);
	sw_service();
}

/** @brief Rebuild the slot table from the current channel configuration. */
void sw_rebuild() {
	k_timer_stop(&sw_timer);
	sw_running = false;
	sw_slot_count = 0;
	sw_slot_idx = 0;
	sw_active_count = 0;

	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (!sw_chan[i].active) {
			continue;
		}
		sw_active_count++;
		const uint32_t h = sw_chan[i].high_us;
		if (h == 0 || h >= sw_period_us) {
			/* Constant level: no edge inside the period. */
			continue;
		}
		/* Insertion sort, skipping duplicates. */
		uint16_t pos = 0;
		while (pos < sw_slot_count && sw_slot[pos] < h) {
			pos++;
		}
		if (pos < sw_slot_count && sw_slot[pos] == h) {
			continue;
		}
		for (uint16_t k = sw_slot_count; k > pos; k--) {
			sw_slot[k] = sw_slot[k - 1];
		}
		sw_slot[pos] = h;
		sw_slot_count++;
	}

	if (sw_active_count == 0 || sw_slot_count == 0) {
		/* Nothing to toggle: park the engine. */
		return;
	}

	sw_slot[sw_slot_count] = sw_period_us; /* period boundary marker */
	sw_slot_count++;

	if (!sw_timer_ready) {
		k_timer_init(&sw_timer, sw_expiry, nullptr);
		sw_timer_ready = true;
	}

	sw_running = true;
	sw_slot_idx = 0;
	sw_now_us = 0;
	/* Raise everything now, then run the falling-edge schedule. */
	sw_raise_all();
	sw_arm_next();
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Driver                                                                    */
/* ------------------------------------------------------------------------- */

UNOQ_PWMServoDriver::UNOQ_PWMServoDriver()
	: _freq(UNOQ_PWM_DEFAULT_FREQ), _period_us(20000), _oscillator_freq(25000000UL),
	  _asleep(false), _sw_count(0) {
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		_on[i] = 0;
		_off[i] = 0;
		_claimed[i] = false;
		_claimResult[i] = 0;
		_pinmuxResult[i] = 0;
		_pwmResult[i] = 0;
	}
}

uint16_t UNOQ_PWMServoDriver::channelCount() {
	return (uint16_t)UNOQ_PWM_PIN_COUNT;
}

bool UNOQ_PWMServoDriver::isHardwarePWM(uint8_t num) {
	return unoq_hw_pwm_index((pin_size_t)num) >= 0;
}

uint32_t UNOQ_PWMServoDriver::softwareTickUs() {
	return 1000000UL / (uint32_t)CONFIG_SYS_CLOCK_TICKS_PER_SEC;
}

int UNOQ_PWMServoDriver::lastPinmuxResult(uint8_t num) const {
	return (num < UNOQ_PWM_PIN_COUNT) ? _pinmuxResult[num] : 0;
}

int UNOQ_PWMServoDriver::lastPwmResult(uint8_t num) const {
	return (num < UNOQ_PWM_PIN_COUNT) ? _pwmResult[num] : 0;
}

bool UNOQ_PWMServoDriver::claimFailed(uint8_t num) const {
	return (num < UNOQ_PWM_PIN_COUNT) ? (_claimResult[num] != 0) : true;
}

bool UNOQ_PWMServoDriver::begin(float freq) {
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		_on[i] = 0;
		_off[i] = 0;
		_claimed[i] = false;
		_claimResult[i] = 0;
		_pinmuxResult[i] = 0;
		_pwmResult[i] = 0;
		sw_chan[i].active = false;
		sw_chan[i].high_us = 0;
	}
	_sw_count = 0;
	_asleep = false;
	_oscillator_freq = 25000000UL;

	if (!sw_timer_ready) {
		k_timer_init(&sw_timer, sw_expiry, nullptr);
		sw_timer_ready = true;
	}
	k_timer_stop(&sw_timer);
	sw_running = false;

	setPWMFreq(freq);
	return true;
}

void UNOQ_PWMServoDriver::end() {
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (_claimed[i]) {
			detach((uint8_t)i);
		}
	}
	k_timer_stop(&sw_timer);
	sw_running = false;
}

void UNOQ_PWMServoDriver::setPWMFreq(float freq) {
	if (freq < UNOQ_PWM_MIN_FREQ) {
		freq = UNOQ_PWM_MIN_FREQ;
	}
	if (freq > UNOQ_PWM_MAX_FREQ) {
		freq = UNOQ_PWM_MAX_FREQ;
	}
	_freq = freq;
	_period_us = (uint32_t)(1000000.0f / freq + 0.5f);
	if (_period_us == 0) {
		_period_us = 1;
	}

	/* The software engine keeps its own copy of the period. Without this it
	 * stays stuck at whatever it was initialised with - 20 ms, i.e. 50 Hz -
	 * and silently ignores every frequency change. On a scope that looks like
	 * the software channels running at 50 Hz while the hardware ones honour
	 * the requested frequency. */
	sw_period_us = _period_us;

	/* Re-program everything that is already running. */
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (!_claimed[i]) {
			continue;
		}
		const uint32_t width = _off[i] > _on[i] ? (uint32_t)(_off[i] - _on[i])
												: (uint32_t)(UNOQ_PWM_RESOLUTION + _off[i] - _on[i]);
		const uint32_t high = (uint32_t)(((uint64_t)_period_us * width) / UNOQ_PWM_RESOLUTION);
		_writeChannel((uint8_t)i, high, _period_us);
	}
}

void UNOQ_PWMServoDriver::reset() {
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (_claimed[i]) {
			_writeChannel((uint8_t)i, 0, _period_us);
			_on[i] = 0;
			_off[i] = 0;
		}
	}
	_freq = UNOQ_PWM_DEFAULT_FREQ;
	_period_us = 20000;
	sw_period_us = _period_us;
}

void UNOQ_PWMServoDriver::sleep() {
	_asleep = true;
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (_claimed[i]) {
			_writeChannel((uint8_t)i, 0, _period_us);
		}
	}
}

void UNOQ_PWMServoDriver::wakeup() {
	if (!_asleep) {
		return;
	}
	_asleep = false;
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (_claimed[i]) {
			const uint32_t width = _off[i] > _on[i] ? (uint32_t)(_off[i] - _on[i])
													: (uint32_t)(UNOQ_PWM_RESOLUTION + _off[i] - _on[i]);
			const uint32_t high = (uint32_t)(((uint64_t)_period_us * width) / UNOQ_PWM_RESOLUTION);
			_writeChannel((uint8_t)i, high, _period_us);
		}
	}
}

bool UNOQ_PWMServoDriver::attach(uint8_t num) {
	return _claim(num);
}

void UNOQ_PWMServoDriver::detach(uint8_t num) {
	if (num >= UNOQ_PWM_PIN_COUNT) {
		return;
	}
	if (_claimed[num]) {
		_writeChannel(num, 0, _period_us);
	}
	_claimed[num] = false;
	_on[num] = 0;
	_off[num] = 0;
	if (sw_chan[num].active) {
		sw_chan[num].active = false;
		sw_chan[num].high_us = 0;
		_sw_count--;
		sw_rebuild();
	}
}

bool UNOQ_PWMServoDriver::attached(uint8_t num) const {
	return num < UNOQ_PWM_PIN_COUNT && _claimed[num];
}

void UNOQ_PWMServoDriver::refresh() {
	/* The engine is interrupt driven; kept for SoftwareServo-style sketches. */
}

uint8_t UNOQ_PWMServoDriver::setPWM(uint8_t num, uint16_t on, uint16_t off) {
	if (num >= UNOQ_PWM_PIN_COUNT) {
		return 1;
	}
	if (!_claim(num)) {
		return 1;
	}

	on &= (UNOQ_PWM_RESOLUTION - 1);
	off &= (UNOQ_PWM_RESOLUTION - 1);

	uint32_t width;
	if (off > on) {
		width = (uint32_t)(off - on);
	} else if (off == on) {
		width = 0;
	} else {
		/* Wrapped window, e.g. on = 4000, off = 200. */
		width = (uint32_t)(UNOQ_PWM_RESOLUTION - on + off);
	}
	_on[num] = on;
	_off[num] = (uint16_t)off;

	const uint32_t high = (uint32_t)(((uint64_t)_period_us * width) / UNOQ_PWM_RESOLUTION);
	if (!_asleep) {
		_writeChannel(num, high, _period_us);
	}
	return 0;
}

void UNOQ_PWMServoDriver::setPin(uint8_t num, uint16_t val, bool invert) {
	if (val > UNOQ_PWM_RESOLUTION) {
		val = UNOQ_PWM_RESOLUTION;
	}
	if (invert) {
		val = (uint16_t)(UNOQ_PWM_RESOLUTION - val);
	}
	(void)setPWM(num, 0, val);
}

uint16_t UNOQ_PWMServoDriver::getPWM(uint8_t num, bool off) {
	if (num >= UNOQ_PWM_PIN_COUNT) {
		return 0;
	}
	return off ? _off[num] : _on[num];
}

void UNOQ_PWMServoDriver::writeMicroseconds(uint8_t num, uint16_t microseconds) {
	if (num >= UNOQ_PWM_PIN_COUNT) {
		return;
	}
	if (!_claim(num)) {
		return;
	}
	if (microseconds > _period_us) {
		microseconds = (uint16_t)_period_us;
	}
	/* Keep the PCA9685-style shadow registers consistent. */
	_off[num] = (uint16_t)(((uint32_t)microseconds * UNOQ_PWM_RESOLUTION) / _period_us);
	_on[num] = 0;
	if (!_asleep) {
		_writeChannel(num, microseconds, _period_us);
	}
}

uint8_t UNOQ_PWMServoDriver::readPrescale() {
	if (_freq <= 0.0f) {
		return PCA9685_PRESCALE_MIN;
	}
	float prescale = (_oscillator_freq / (UNOQ_PWM_RESOLUTION * _freq)) - 1.0f;
	if (prescale < PCA9685_PRESCALE_MIN) {
		prescale = PCA9685_PRESCALE_MIN;
	}
	if (prescale > PCA9685_PRESCALE_MAX) {
		prescale = PCA9685_PRESCALE_MAX;
	}
	return (uint8_t)(prescale + 0.5f);
}

void UNOQ_PWMServoDriver::setOscillatorFrequency(uint32_t freq) {
	_oscillator_freq = freq;
}

uint32_t UNOQ_PWMServoDriver::getOscillatorFrequency() {
	return _oscillator_freq;
}

/* ------------------------------------------------------------------------- */
/* Internals                                                                 */
/* ------------------------------------------------------------------------- */

bool UNOQ_PWMServoDriver::_claim(uint8_t num) {
	if (num >= UNOQ_PWM_PIN_COUNT) {
		return false;
	}
	if (_claimed[num]) {
		return true;
	}

	_claimResult[num] = 0;
	_pinmuxResult[num] = 0;
	_pwmResult[num] = 0;

	const int hw = unoq_hw_pwm_index((pin_size_t)num);
	if (hw >= 0) {
		/* Order matters: the PWM nodes are declared with "zephyr,deferred-init",
		 * so the device is NOT initialised at boot. The core's pinctrl helper
		 * both initialises it and routes the pin, which is why analogWrite()
		 * calls it *before* pwm_is_ready_dt(). Checking readiness first would
		 * always fail and the channel would silently stay dead. */
		_pinmuxResult[num] = (int16_t)unoq_pwm_apply_pinmux((pin_size_t)num,
														   unoq_hw_pwm[hw].dev, (size_t)hw);
		if (!pwm_is_ready_dt(&unoq_hw_pwm[hw])) {
			_claimResult[num] = -1; /* still not usable after the init attempt */
			return false;
		}
	} else {
		/* Plain GPIO: the core's pinMode() is all that is needed, and it is the
		 * only portable call - the older core's internal pinmux helper is gone
		 * from newer releases. */
		pinMode((pin_size_t)num, OUTPUT);
		(void)gpio_pin_set_dt(&unoq_gpio[num], 0);
		sw_chan[num].active = true;
		sw_chan[num].high_us = 0;
		_sw_count++;
	}

	_claimed[num] = true;
	_on[num] = 0;
	_off[num] = 0;
	return true;
}

void UNOQ_PWMServoDriver::_applyHardware(uint8_t num, uint32_t high_us, uint32_t period_us) {
	const int idx = unoq_hw_pwm_index((pin_size_t)num);
	if (idx < 0) {
		return;
	}
	uint32_t period_ns = period_us * 1000UL;
	uint32_t pulse_ns = high_us * 1000UL;
	if (pulse_ns > period_ns) {
		pulse_ns = period_ns;
	}
	/* The 'N' timer outputs and the common-anode LEDs are wired inverted, so a
	 * caller-facing "time HIGH" has to be turned into the complementary pulse. */
	if (UNOQ_PWM_FLAG_INVERTED(idx)) {
		pulse_ns = period_ns - pulse_ns;
	}
	/* Keep the result: an out-of-range period or an unroutable channel shows up
	 * here and nowhere else, which otherwise looks exactly like a dead pin. */
	_pwmResult[num] = (int16_t)pwm_set_dt(&unoq_hw_pwm[idx], period_ns, pulse_ns);
}

void UNOQ_PWMServoDriver::_swApply(uint8_t num, uint32_t high_us) {
	if (!sw_chan[num].active) {
		return;
	}
	sw_chan[num].high_us = high_us;
	if (high_us >= sw_period_us) {
		/* Constant HIGH. */
		(void)gpio_pin_set_dt(&unoq_gpio[num], 1);
	} else if (high_us == 0) {
		(void)gpio_pin_set_dt(&unoq_gpio[num], 0);
	}
	sw_rebuild();
}

bool UNOQ_PWMServoDriver::_writeChannel(uint8_t num, uint32_t high_us, uint32_t period_us) {
	const int hw = unoq_hw_pwm_index((pin_size_t)num);
	if (hw >= 0) {
		_applyHardware(num, high_us, period_us);
		return true;
	}
	_swApply(num, high_us);
	return true;
}
