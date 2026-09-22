/*
    UNOQ_PWMServo - implementation.

    SPDX-License-Identifier: MPL-2.0
*/

#include "UNOQ_PWMServo.h"

namespace {

/* Lazily brought up on the first attach(), mirroring how servo libraries grab
 * their timer resources only when they are actually needed. */
UNOQ_PWMServoDriver servo_pwm;
bool servo_pwm_ready = false;

} // namespace

UNOQ_PWMServoDriver &UNOQ_PWMServo::driver() {
	if (!servo_pwm_ready) {
		servo_pwm.begin(1000000.0f / SERVO_PERIOD_US);
		servo_pwm_ready = true;
	}
	return servo_pwm;
}

UNOQ_PWMServo::UNOQ_PWMServo() : _pin(-1), _min(SERVO_MIN_US), _max(SERVO_MAX_US), _pulse(0) {}

uint8_t UNOQ_PWMServo::attach(int pin, int min, int max) {
	if (pin < 0 || pin >= (int)UNOQ_PWMServoDriver::channelCount()) {
		return INVALID_SERVO;
	}
	if (!driver().attach((uint8_t)pin)) {
		return INVALID_SERVO;
	}

	_min = (uint32_t)constrain(min, 0, SERVO_PERIOD_US);
	_max = (uint32_t)constrain(max, 0, SERVO_PERIOD_US);
	if (_max <= _min) {
		_max = _min + 1;
	}
	_pin = pin;
	_pulse = _min;
	driver().writeMicroseconds((uint8_t)_pin, (uint16_t)_pulse);
	return (uint8_t)pin;
}

void UNOQ_PWMServo::detach() {
	if (!attached()) {
		return;
	}
	driver().detach((uint8_t)_pin);
	_pin = -1;
}

void UNOQ_PWMServo::write(int value) {
	if (!attached()) {
		return;
	}
	value = constrain(value, 0, 180);
	_pulse = map((uint32_t)value, 0, 180, _min, _max);
	driver().writeMicroseconds((uint8_t)_pin, (uint16_t)_pulse);
}

void UNOQ_PWMServo::writeMicroseconds(int value) {
	if (!attached()) {
		return;
	}
	_pulse = (uint32_t)constrain(value, (int)_min, (int)_max);
	driver().writeMicroseconds((uint8_t)_pin, (uint16_t)_pulse);
}

int UNOQ_PWMServo::read() {
	if (!attached()) {
		return -1;
	}
	return (int)map(_pulse, _min, _max, 0, 180);
}

int UNOQ_PWMServo::readMicroseconds() {
	if (!attached()) {
		return -1;
	}
	return (int)_pulse;
}

bool UNOQ_PWMServo::attached() {
	return _pin >= 0;
}
