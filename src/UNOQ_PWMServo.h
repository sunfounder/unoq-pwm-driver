/*
    UNOQ_PWMServo - servo helper on top of UNOQ_PWMServoDriver.

    The interface matches Arduino_HardwareServo (and therefore the classic
    Servo library) so sketches can be moved over unchanged.  Because the UNO Q
    can emit PWM on any pin, unlike the timer-bound Servo implementations there
    is no limit on how many servos may be attached.

    SPDX-License-Identifier: MPL-2.0
*/

#ifndef UNOQ_PWMSERVO_H
#define UNOQ_PWMSERVO_H

#include <Arduino.h>

#include "UNOQ_PWMServoDriver.h"

#define SERVO_PERIOD_US 20000 /* 20 ms = 50 Hz */
#define SERVO_MIN_US 500      /* 0.5 ms  ->   0 deg */
#define SERVO_MAX_US 2500     /* 2.5 ms  -> 180 deg */

#define INVALID_SERVO 255

/**
 * @brief Single servo output.
 *
 * All instances share one internal UNOQ_PWMServoDriver, so they also share the
 * 50 Hz frame rate, exactly like a multi-channel servo controller does.
 */
class UNOQ_PWMServo {
  public:
	UNOQ_PWMServo();

	/**
	 * @brief Attach the servo to a pin and start emitting pulses.
	 * @param pin Arduino pin number; any pin of the UNO Q will work.
	 * @param min Pulse width in microseconds for 0 degrees.
	 * @param max Pulse width in microseconds for 180 degrees.
	 * @return The pin number on success, INVALID_SERVO on failure.
	 */
	uint8_t attach(int pin, int min = SERVO_MIN_US, int max = SERVO_MAX_US);

	/** @brief Stop driving the pin and release it. */
	void detach();

	/** @brief Move to an angle in degrees; values outside 0..180 are clamped. */
	void write(int value);

	/** @brief Set the pulse width directly, in microseconds. */
	void writeMicroseconds(int value);

	/** @brief Last commanded angle in degrees, or -1 when detached. */
	int read();

	/** @brief Last commanded pulse width in microseconds, or -1 when detached. */
	int readMicroseconds();

	/** @brief True while the servo is attached to a pin. */
	bool attached();

	/** @brief The driver shared by every servo instance. */
	static UNOQ_PWMServoDriver &driver();

  private:
	int _pin;
	uint32_t _min;
	uint32_t _max;
	uint32_t _pulse;
};

#endif /* UNOQ_PWMSERVO_H */
