/*
    HardwareServo - servo output built on HardwareAnalogWrite.

    The interface matches Arduino_HardwareServo (and therefore the classic
    Servo library) so sketches can be moved over unchanged. Because the UNO Q
    can emit PWM on any pin, there is no cap on how many servos may be
    attached, unlike the timer-bound Servo implementations.

    All servos share one 50 Hz frame rate, which is what the hardware allows:
    channels on the same STM32 timer share a prescaler.

    SPDX-License-Identifier: MPL-2.0
*/

#ifndef HARDWARESERVO_H
#define HARDWARESERVO_H

#include <Arduino.h>

#include "HardwareAnalogWrite.h"

#define SERVO_PERIOD_US 20000 /* 20 ms = 50 Hz */
#define SERVO_MIN_US 500      /* 0.5 ms ->   0 deg */
#define SERVO_MAX_US 2500     /* 2.5 ms -> 180 deg */

#define INVALID_SERVO 255 /* flag indicating an invalid servo index */

class HardwareServo {
  public:
	HardwareServo();

	/**
	 * @brief Attach the servo to a pin and start emitting pulses.
	 *
	 * This method sets the appropriate pinMode and configures the minimum and
	 * maximum pulse width values.
	 *
	 * @param pin The GPIO pin number the servo is connected to.
	 * @param min The pulse width in microseconds for 0 degrees.
	 * @param max The pulse width in microseconds for 180 degrees.
	 * @return uint8_t The pin number on success, or INVALID_SERVO on failure.
	 */
	uint8_t attach(int pin, int min = SERVO_MIN_US, int max = SERVO_MAX_US);

	/** @brief Detach the servo from its pin and release the channel. */
	void detach();

	/**
	 * @brief Set the servo to a specific angle.
	 * @param value The desired angle in degrees, 0 to 180. Out-of-range values
	 *              are clamped.
	 */
	void write(int value);

	/** @brief Set the pulse width directly, in microseconds. */
	void writeMicroseconds(int value);

	/** @brief The current position as an angle, or -1 when detached. */
	int read();

	/** @brief The current pulse width in microseconds, or -1 when detached. */
	int readMicroseconds();

	/** @brief True while the servo is attached to a pin. */
	bool attached();

  private:
	int _pin;
	uint32_t _min;
	uint32_t _max;
	uint32_t _pulse;
};

#endif /* HARDWARESERVO_H */
