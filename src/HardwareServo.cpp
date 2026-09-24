/*
    HardwareServo - implementation.

    SPDX-License-Identifier: MPL-2.0
*/

#include "HardwareServo.h"

namespace {

/* The servo frame is 20 ms = 50 Hz on both engines. */
const float SERVO_FREQ_HZ = 1000000.0f / SERVO_PERIOD_US;

/** @brief Microseconds of the current frame to an 8 bit duty value. */
uint8_t us_to_duty(uint32_t us) {
	if (us >= SERVO_PERIOD_US) {
		return ANALOG_WRITE_MAX;
	}
	return (uint8_t)(((uint64_t)us * ANALOG_WRITE_MAX + SERVO_PERIOD_US / 2) / SERVO_PERIOD_US);
}

/** @brief Put both engines on the servo frame rate. */
void ensure_servo_rate(int pin) {
	if (analogWritePinFrequency((pin_size_t)pin) != SERVO_FREQ_HZ) {
		if (!analogWriteFrequency((pin_size_t)pin, SERVO_FREQ_HZ)) {
			/* No timer behind the pin: the software engine carries it. */
			analogWriteFrequency(SERVO_FREQ_HZ);
		}
	}
}

} // namespace

HardwareServo::HardwareServo() : _pin(-1), _min(SERVO_MIN_US), _max(SERVO_MAX_US), _pulse(0) {}

uint8_t HardwareServo::attach(int pin, int min, int max) {
	if (pin < 0 || pin >= (int)UNOQ_PWM_PIN_COUNT) {
		return INVALID_SERVO;
	}

	_min = (uint32_t)constrain(min, 0, SERVO_PERIOD_US - 1);
	_max = (uint32_t)constrain(max, 1, SERVO_PERIOD_US);
	if (_max <= _min) {
		_max = _min + 1;
	}

	ensure_servo_rate(pin);

	_pin = pin;
	_pulse = _min;
	if (!analogWritePin((pin_size_t)_pin, us_to_duty(_pulse))) {
		_pin = -1;
		return INVALID_SERVO;
	}
	return (uint8_t)pin;
}

void HardwareServo::detach() {
	if (!attached()) {
		return;
	}
	analogWriteStop((pin_size_t)_pin);
	_pin = -1;
}

void HardwareServo::write(int value) {
	if (!attached()) {
		return;
	}
	value = constrain(value, 0, 180);
	_pulse = map((uint32_t)value, 0, 180, _min, _max);
	analogWritePin((pin_size_t)_pin, us_to_duty(_pulse));
}

void HardwareServo::writeMicroseconds(int value) {
	if (!attached()) {
		return;
	}
	_pulse = (uint32_t)constrain(value, (int)_min, (int)_max);
	analogWritePin((pin_size_t)_pin, us_to_duty(_pulse));
}

int HardwareServo::read() {
	if (!attached()) {
		return -1;
	}
	return (int)map(_pulse, _min, _max, 0, 180);
}

int HardwareServo::readMicroseconds() {
	if (!attached()) {
		return -1;
	}
	return (int)_pulse;
}

bool HardwareServo::attached() {
	return _pin >= 0;
}
