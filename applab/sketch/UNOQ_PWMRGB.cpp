/*
    UNOQ_PWMRGB - implementation.

    SPDX-License-Identifier: MPL-2.0
*/

#include "UNOQ_PWMRGB.h"

namespace {

/* Shared by every UNOQ_PWMRGB instance, exactly like UNOQ_PWMServo shares one
 * driver. Brought up on first use so that a sketch which never touches the
 * LEDs pays nothing for them. */
UNOQ_PWMServoDriver rgb_pwm;
bool rgb_pwm_ready = false;

} // namespace

UNOQ_PWMServoDriver &UNOQ_PWMRGB::driver() {
	if (!rgb_pwm_ready) {
		rgb_pwm.begin(UNOQ_RGB_DEFAULT_FREQ);
		rgb_pwm_ready = true;
	}
	return rgb_pwm;
}

UNOQ_PWMRGB::UNOQ_PWMRGB(Which which) : _which(which), _begun(false) {
	/* unoq_builtin_led_pin[] lists the six channels as
	 *   LED3_R, LED3_G, LED3_B, LED4_R, LED4_G, LED4_B
	 * which is the same order the core's LED3_R..LED4_B macros use. */
	const uint8_t base = (which == LED4) ? 3 : 0;
	for (uint8_t i = 0; i < 3; i++) {
		const uint8_t idx = base + i;
		_pins[i] = (idx < UNOQ_BUILTIN_LED_COUNT) ? (uint8_t)unoq_builtin_led_pin[idx] : 255;
	}
}

uint8_t UNOQ_PWMRGB::ledCount() {
	return (uint8_t)(UNOQ_BUILTIN_LED_COUNT / 3);
}

bool UNOQ_PWMRGB::available() {
	return UNOQ_BUILTIN_LED_COUNT >= 6;
}

uint8_t UNOQ_PWMRGB::pin(uint8_t channel) const {
	if (channel > BLUE) {
		return 255;
	}
	return _pins[channel];
}

bool UNOQ_PWMRGB::isHardwarePWM() const {
	if (_pins[RED] == 255) {
		return false;
	}
	return UNOQ_PWMServoDriver::isHardwarePWM(_pins[RED]);
}

bool UNOQ_PWMRGB::begin(float freq) {
	if (!available()) {
		return false;
	}

	UNOQ_PWMServoDriver &pwm = driver();
	pwm.setPWMFreq(freq);

	bool ok = true;
	for (uint8_t i = 0; i < 3; i++) {
		if (!pwm.attach(_pins[i])) {
			ok = false;
		}
		pwm.setPin(_pins[i], 0);
	}

	_begun = ok;
	return ok;
}

void UNOQ_PWMRGB::setChannel(uint8_t channel, uint8_t value) {
	if (channel > BLUE || _pins[channel] == 255) {
		return;
	}
	if (!_begun && !begin()) {
		return;
	}
	/* 0..255 -> the driver's 12-bit window, so that 255 is exactly full on. */
	const uint16_t duty = (uint16_t)(((uint32_t)value * (UNOQ_PWM_RESOLUTION - 1)) / UNOQ_RGB_MAX_VALUE);
	driver().setPin(_pins[channel], duty);
}

void UNOQ_PWMRGB::setColor(uint8_t red, uint8_t green, uint8_t blue) {
	if (!_begun && !begin()) {
		return;
	}
	setChannel(RED, red);
	setChannel(GREEN, green);
	setChannel(BLUE, blue);
}

void UNOQ_PWMRGB::setColor(uint32_t rgb) {
	setColor((uint8_t)((rgb >> 16) & 0xFF), (uint8_t)((rgb >> 8) & 0xFF), (uint8_t)(rgb & 0xFF));
}
