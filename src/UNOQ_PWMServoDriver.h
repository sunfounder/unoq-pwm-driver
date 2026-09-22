/*
    UNOQ_PWMServoDriver - full-pin PWM driver for the Arduino UNO Q.

    The UNO Q exposes 70 GPIOs to sketches but only 16 of them are wired to an
    STM32 timer channel. This driver makes *every* pin able to emit PWM:

      * pins with a timer channel  -> hardware PWM, exact frequency,
                                      1 us pulse resolution, zero CPU cost
      * all other GPIOs            -> software PWM, generated from a Zephyr
                                      k_timer (100 us granularity on the UNO Q)

    The public surface follows Adafruit_PWMServoDriver so existing sketches and
    documentation for the PCA9685 break-out remain recognisable, with the
    obvious difference that a "channel" here IS an Arduino pin number rather
    than an I2C expander output.

    SPDX-License-Identifier: MPL-2.0
*/

#ifndef UNOQ_PWMSERVODRIVER_H
#define UNOQ_PWMSERVODRIVER_H

#include <Arduino.h>

#include "UNOQ_PWMPins.h"

/** Default PWM frequency, i.e. the 50 Hz servo frame rate. */
#define UNOQ_PWM_DEFAULT_FREQ 50.0f

/** Number of duty steps, matching the PCA9685's 12-bit counters. */
#define UNOQ_PWM_RESOLUTION 4096

/** Lowest / highest frequency the engine will accept, in Hz. */
#define UNOQ_PWM_MIN_FREQ 1.0f
#define UNOQ_PWM_MAX_FREQ 2000.0f

/* PCA9685 prescaler limits, kept so readPrescale() stays source compatible. */
#define PCA9685_PRESCALE_MIN 3
#define PCA9685_PRESCALE_MAX 255

/**
 * @brief Full-pin PWM driver for the Arduino UNO Q.
 *
 * A "channel" is an Arduino pin number, so channel 13 is D13 and channel 50 is
 * the red channel of the on-board RGB LED. All UNOQ_PWM_PIN_COUNT pins are
 * addressable.
 */
class UNOQ_PWMServoDriver {
  public:
	UNOQ_PWMServoDriver();

	/**
	 * @brief Initialise the driver.
	 *
	 * Does not touch any pin yet: pins are claimed lazily by the first
	 * setPWM()/setPin()/writeMicroseconds() call, exactly like the servo
	 * libraries do on attach().
	 *
	 * @param freq Initial PWM frequency in Hz (default 50 Hz).
	 * @return true when at least the hardware PWM devices are ready.
	 */
	bool begin(float freq = UNOQ_PWM_DEFAULT_FREQ);

	/** @brief Release every channel and stop the software PWM engine. */
	void end();

	/* ---------------- Adafruit_PWMServoDriver compatible API --------------- */

	/** @brief Stop all outputs and reset the frequency to the default. */
	void reset();

	/** @brief Drive every channel to 0 and stop the software engine. */
	void sleep();

	/** @brief Resume normal output after sleep(). */
	void wakeup();

	/**
	 * @brief Set the PWM frequency shared by all channels.
	 *
	 * The PCA9685 has a single oscillator for all 16 outputs; the UNO Q
	 * behaves the same way because several header pins share one timer.
	 * @param freq Frequency in Hz, clamped to [UNOQ_PWM_MIN_FREQ, UNOQ_PWM_MAX_FREQ].
	 */
	void setPWMFreq(float freq);

	/**
	 * @brief Set the PWM duty cycle of one channel.
	 *
	 * @param num Channel, i.e. Arduino pin number.
	 * @param on  Start tick, 0..UNOQ_PWM_RESOLUTION-1.
	 * @param off End tick, 0..UNOQ_PWM_RESOLUTION. When off > on the output is
	 *            HIGH for (off-on) ticks; off <= on is treated as fully off.
	 * @return 0 on success, 1 on an unknown channel.
	 *
	 * @note Only the pulse *width* is reproduced: MCU timers cannot emit an
	 *       arbitrary phase offset the way the PCA9685 can, so the non-zero
	 *       `on` value only shifts where the duty window starts on software
	 *       channels and is ignored on hardware channels.
	 */
	uint8_t setPWM(uint8_t num, uint16_t on, uint16_t off);

	/**
	 * @brief Convenience wrapper mirroring the PCA9685 helper.
	 * @param num    Channel, i.e. Arduino pin number.
	 * @param val    0..UNOQ_PWM_RESOLUTION-1 duty value.
	 * @param invert When true the duty cycle is inverted.
	 */
	void setPin(uint8_t num, uint16_t val, bool invert = false);

	/** @brief Read back the last value programmed on a channel. */
	uint16_t getPWM(uint8_t num, bool off = false);

	/**
	 * @brief Set a pulse width in microseconds, keeping the current frequency.
	 *
	 * This is the servo-style entry point: pulse widths above the period are
	 * clamped to the period.
	 */
	void writeMicroseconds(uint8_t num, uint16_t microseconds);

	/**
	 * @brief PCA9685 compatibility shim.
	 *
	 * The UNO Q has no prescaler register; the value is derived from the
	 * current frequency and the nominal 25 MHz oscillator so that code
	 * written for the PCA9685 keeps working.
	 */
	uint8_t readPrescale();

	/** @brief PCA9685 compatibility shim; only affects readPrescale(). */
	void setOscillatorFrequency(uint32_t freq);
	uint32_t getOscillatorFrequency();

	/* ---------------------------- extensions ------------------------------ */

	/** @brief Number of addressable channels (== number of Arduino pins). */
	static uint16_t channelCount();

	/** @brief True when the channel is backed by a real timer channel. */
	static bool isHardwarePWM(uint8_t num);

	/** @brief Current PWM frequency in Hz. */
	float getPWMFreq() const { return _freq; }

	/** @brief Claim a channel without changing its output level. */
	bool attach(uint8_t num);

	/** @brief Release a channel; hardware output is left LOW. */
	void detach(uint8_t num);

	/** @brief True when the channel is currently claimed. */
	bool attached(uint8_t num) const;

	/** @brief Granularity of the software PWM engine, in microseconds. */
	static uint32_t softwareTickUs();

	/** @brief Number of channels currently served by software PWM. */
	uint16_t softwareChannelCount() const { return _sw_count; }

	/**
	 * @brief Service the software PWM engine.
	 *
	 * The engine is interrupt driven and does not need to be polled; the hook
	 * exists so that sketches written for SoftwareServo-style libraries, which
	 * do call refresh() from loop(), keep compiling.
	 */
	void refresh();

  private:
	float _freq;
	uint32_t _period_us;
	uint32_t _oscillator_freq;
	bool _asleep;

	uint16_t _on[UNOQ_PWM_PIN_COUNT];
	uint16_t _off[UNOQ_PWM_PIN_COUNT];
	bool _claimed[UNOQ_PWM_PIN_COUNT];

	/* Number of claimed channels currently served by software PWM. */
	uint16_t _sw_count;

	bool _claim(uint8_t num);
	bool _writeChannel(uint8_t num, uint32_t high_us, uint32_t period_us);
	void _applyHardware(uint8_t num, uint32_t high_us, uint32_t period_us);
	void _swApply(uint8_t num, uint32_t high_us);
};

#endif /* UNOQ_PWMSERVODRIVER_H */
