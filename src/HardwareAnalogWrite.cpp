/*
    HardwareAnalogWrite - implementation.

    Two engines sit behind one API:

      * Hardware engine - the pins listed in the board devicetree "pwms"
        property are driven with pwm_set_dt(): exact frequency, 1 us pulse
        resolution, no CPU cost. Channels sharing an STM32 timer also share a
        prescaler, so the hardware frequency is global.

      * Software engine - every other GPIO is driven from a single Zephyr
        k_timer used in one-shot mode. All active software channels rise
        together at the start of a period, then the timer is re-armed once per
        distinct pulse width to drop the matching pins low again. Granularity
        is one kernel tick, 100 us on the UNO Q.

    Both engines store a single per-pin duty value (0..255), so changing a
    frequency can re-apply the same duty without the caller having to repeat
    the analogWrite() calls.

    SPDX-License-Identifier: MPL-2.0
*/

#include "HardwareAnalogWrite.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

namespace {

/* ------------------------------- state -------------------------------- */

bool started[UNOQ_PWM_PIN_COUNT];
uint8_t duty[UNOQ_PWM_PIN_COUNT]; /* Last value written, 0..255. */

float hw_freq = ANALOG_WRITE_DEFAULT_FREQ;
float sw_freq = ANALOG_WRITE_DEFAULT_FREQ;
uint32_t hw_period_us = 1000;
uint32_t sw_period_us = 1000;

float clamp_freq(float hz) {
	if (!(hz > ANALOG_WRITE_MIN_FREQ)) { /* also catches NaN */
		return ANALOG_WRITE_MIN_FREQ;
	}
	if (hz > ANALOG_WRITE_MAX_FREQ) {
		return ANALOG_WRITE_MAX_FREQ;
	}
	return hz;
}

uint32_t period_from_freq(float hz) {
	const uint32_t us = (uint32_t)(1000000.0f / hz + 0.5f);
	return us == 0 ? 1 : us;
}

/** @brief 0..255 duty to microseconds of the given period. */
uint32_t duty_to_us(uint8_t value, uint32_t period_us) {
	return (uint32_t)(((uint64_t)period_us * value) / ANALOG_WRITE_MAX);
}

/* --------------------------- software engine --------------------------- */

struct SwChannel {
	bool active;
};

SwChannel sw_chan[UNOQ_PWM_PIN_COUNT];
uint32_t sw_slot[UNOQ_PWM_PIN_COUNT + 1];
uint16_t sw_slot_count = 0;
uint16_t sw_slot_idx = 0;
uint16_t sw_engine_count = 0;
uint32_t sw_now_us = 0;
bool sw_running = false;

struct k_timer sw_timer;
bool sw_timer_ready = false;

void sw_arm_next();

uint32_t sw_high_us(pin_size_t pin) {
	const uint32_t h = duty_to_us(duty[pin], sw_period_us);
	/* A duty of 0 or 255 is a constant level, not an edge inside the period. */
	if (duty[pin] == 0) {
		return 0;
	}
	if (duty[pin] >= ANALOG_WRITE_MAX) {
		return sw_period_us;
	}
	return h == 0 ? 1 : h;
}

void sw_raise_all() {
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (!sw_chan[i].active) {
			continue;
		}
		const uint32_t h = sw_high_us((pin_size_t)i);
		if (h > 0 && h < sw_period_us) {
			(void)gpio_pin_set_dt(&unoq_gpio[i], 1);
		}
	}
}

void sw_arm_next() {
	if (!sw_running || sw_slot_idx >= sw_slot_count) {
		return;
	}
	const uint32_t next = sw_slot[sw_slot_idx];
	if (next <= sw_now_us) {
		return; /* Never re-arm with a zero or negative delta. */
	}
	k_timer_start(&sw_timer, K_USEC(next - sw_now_us), K_NO_WAIT);
}

void sw_service() {
	sw_now_us = sw_slot[sw_slot_idx];

	if (sw_now_us >= sw_period_us) {
		sw_now_us = 0; /* Period boundary: start the next frame. */
		sw_slot_idx = 0;
		sw_raise_all();
		sw_arm_next();
		return;
	}

	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (sw_chan[i].active && sw_high_us((pin_size_t)i) == sw_now_us) {
			(void)gpio_pin_set_dt(&unoq_gpio[i], 0);
		}
	}
	sw_slot_idx++;
	sw_arm_next();
}

void sw_expiry(struct k_timer *timer) {
	ARG_UNUSED(timer);
	sw_service();
}

/** @brief Rebuild the falling-edge schedule from the current duties. */
void sw_rebuild() {
	k_timer_stop(&sw_timer);
	sw_running = false;
	sw_slot_count = 0;
	sw_slot_idx = 0;

	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (!sw_chan[i].active) {
			continue;
		}
		const uint32_t h = sw_high_us((pin_size_t)i);
		if (h == 0 || h >= sw_period_us) {
			continue; /* Constant level: park this pin. */
		}
		uint16_t pos = 0; /* Insertion sort, skipping duplicates. */
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

	if (sw_engine_count == 0 || sw_slot_count == 0) {
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
	sw_raise_all();
	sw_arm_next();
}

/** @brief Drive the level of a pin whose duty is a constant 0 or 255. */
void sw_apply_constant(pin_size_t pin) {
	const uint32_t h = sw_high_us(pin);
	if (h == 0) {
		(void)gpio_pin_set_dt(&unoq_gpio[pin], 0);
	} else if (h >= sw_period_us) {
		(void)gpio_pin_set_dt(&unoq_gpio[pin], 1);
	}
}

bool sw_claim(pin_size_t pin) {
	if (pin >= UNOQ_PWM_PIN_COUNT || sw_chan[pin].active) {
		return false;
	}
	pinMode(pin, OUTPUT);
	(void)gpio_pin_set_dt(&unoq_gpio[pin], 0);
	sw_chan[pin].active = true;
	sw_engine_count++;
	return true;
}

void sw_release(pin_size_t pin) {
	if (pin >= UNOQ_PWM_PIN_COUNT || !sw_chan[pin].active) {
		return;
	}
	sw_chan[pin].active = false;
	(void)gpio_pin_set_dt(&unoq_gpio[pin], 0);
	if (sw_engine_count > 0) {
		sw_engine_count--;
	}
	sw_rebuild();
}

/* --------------------------- hardware engine --------------------------- */

uint32_t hw_high_us(pin_size_t pin) {
	return duty_to_us(duty[pin], hw_period_us);
}

void hw_write(pin_size_t pin) {
	const int idx = unoq_hw_pwm_index(pin);
	if (idx < 0) {
		return;
	}
	uint32_t period_ns = hw_period_us * 1000UL;
	uint32_t pulse_ns = hw_high_us(pin) * 1000UL;
	if (pulse_ns > period_ns) {
		pulse_ns = period_ns;
	}
	/* Inverted outputs need the caller's "time HIGH" turned into the
	 * complementary pulse so that duty keeps its usual meaning. */
	if (unoq_hw_pwm_inverted((size_t)idx)) {
		pulse_ns = period_ns - pulse_ns;
	}
	(void)pwm_set_dt(&unoq_hw_pwm[idx], period_ns, pulse_ns);
}

/**
 * @brief Claim a timer-backed pin and route it to its PWM device.
 *
 * Order matters: the PWM nodes are declared "zephyr,deferred-init", so they are
 * not initialised at boot. The core's pinctrl helper both initialises the
 * device and routes the pin, which is why it is called *before* checking
 * pwm_is_ready_dt() rather than after.
 */
bool hw_claim(pin_size_t pin, int idx) {
	(void)unoq_pwm_apply_pinmux(pin, unoq_hw_pwm[idx].dev, (size_t)idx);
	return pwm_is_ready_dt(&unoq_hw_pwm[idx]);
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

bool analogWritePin(pin_size_t pin, int value) {
	if (pin >= UNOQ_PWM_PIN_COUNT) {
		return false;
	}

	const int idx = unoq_hw_pwm_index(pin);
	const int clamped = value < 0 ? 0 : (value > ANALOG_WRITE_MAX ? ANALOG_WRITE_MAX : value);

	if (idx >= 0) {
		if (!started[pin]) {
			if (!hw_claim(pin, idx)) {
				return false; /* Routed, but the device is still not usable. */
			}
			started[pin] = true;
		}
		duty[pin] = (uint8_t)clamped;
		hw_write(pin);
		return true;
	}

	if (!started[pin]) {
		if (!sw_claim(pin)) {
			return false;
		}
		started[pin] = true;
	}
	duty[pin] = (uint8_t)clamped;
	sw_apply_constant(pin);
	sw_rebuild();
	return true;
}

bool analogWriteFrequency(pin_size_t pin, float hz) {
	if (pin >= UNOQ_PWM_PIN_COUNT || unoq_hw_pwm_index(pin) < 0) {
		return false; /* Plain GPIO: the software frequency applies. */
	}
	hw_freq = clamp_freq(hz);
	hw_period_us = period_from_freq(hw_freq);
	/* Re-apply the same duty on the new period. */
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (started[i] && unoq_hw_pwm_index((pin_size_t)i) >= 0) {
			hw_write((pin_size_t)i);
		}
	}
	return true;
}

float analogWriteFrequency(float hz) {
	sw_freq = clamp_freq(hz);
	sw_period_us = period_from_freq(sw_freq);
	/* sw_high_us() derives from duty and the period, so the caller's duty is
	 * preserved without rescaling anything here. */
	sw_rebuild();
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		if (sw_chan[i].active) {
			sw_apply_constant((pin_size_t)i);
		}
	}
	return sw_freq;
}

float analogWriteFrequency() {
	return hw_freq;
}

float analogWritePinFrequency(pin_size_t pin) {
	if (pin >= UNOQ_PWM_PIN_COUNT) {
		return 0.0f;
	}
	return unoq_hw_pwm_index(pin) >= 0 ? hw_freq : sw_freq;
}

void analogWriteStop(pin_size_t pin) {
	if (pin >= UNOQ_PWM_PIN_COUNT) {
		return;
	}
	const int idx = unoq_hw_pwm_index(pin);
	if (idx >= 0) {
		if (started[pin]) {
			duty[pin] = 0;
			hw_write(pin); /* Leave the pin at 0 % duty. */
			started[pin] = false;
		}
		return;
	}
	sw_release(pin);
	started[pin] = false;
}

bool analogWriteStarted(pin_size_t pin) {
	return pin < UNOQ_PWM_PIN_COUNT && started[pin];
}

uint32_t analogWriteTickUs() {
	return 1000000UL / (uint32_t)CONFIG_SYS_CLOCK_TICKS_PER_SEC;
}

uint16_t analogWriteSoftwareChannelCount() {
	return sw_engine_count;
}

void analogWriteEnd() {
	for (size_t i = 0; i < UNOQ_PWM_PIN_COUNT; i++) {
		analogWriteStop((pin_size_t)i);
	}
	k_timer_stop(&sw_timer);
	sw_running = false;
}
