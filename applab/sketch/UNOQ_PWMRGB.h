/*
    UNOQ_PWMRGB - colour helper for the on-board RGB LEDs of the Arduino UNO Q.

    The UNO Q carries four RGB LEDs, split across its two processors:

      LED1, LED2  Qualcomm QRB2210 (Linux).  Driven by arduino-app-cli as app
                  status indicators and reachable only from the Linux side,
                  never from a sketch.
      LED3        STM32U585, PH10/PH11/PH12 = TIM5_CH1/2/3.  Hardware PWM.
      LED4        STM32U585, PH13/PH14/PH15.  No timer channel, so it is
                  driven by the driver's software PWM engine, which gives it
                  real dimming that the core's own digitalWrite example
                  cannot offer.

    The channels are resolved from the board devicetree, so the same source
    works on every variant that exposes builtin-led-gpios.

    SPDX-License-Identifier: MPL-2.0
*/

#ifndef UNOQ_PWMRGB_H
#define UNOQ_PWMRGB_H

#include <Arduino.h>

#include "UNOQ_PWMServoDriver.h"

/** Default PWM frequency for the on-board LEDs, in Hz. */
#define UNOQ_RGB_DEFAULT_FREQ 1000.0f

/** A colour value spans 0..255 per channel, like analogWrite(). */
#define UNOQ_RGB_MAX_VALUE 255

/**
 * @brief Drives one of the two MCU-side RGB LEDs of the UNO Q.
 *
 * All LED instances share one underlying UNOQ_PWMServoDriver, so they also
 * share the PWM frequency with any servo attached to the same sketch.
 */
class UNOQ_PWMRGB {
  public:
	/** @brief The MCU-side RGB LEDs. LED1 and LED2 are not accessible here. */
	enum Which { LED3 = 0, LED4 = 1 };

	/** @brief Colour channel index accepted by setChannel() and pin(). */
	enum Channel { RED = 0, GREEN = 1, BLUE = 2 };

	/**
	 * @brief Bind to one of the on-board LEDs.
	 * @param which LED3 (hardware PWM) or LED4 (software PWM).
	 */
	explicit UNOQ_PWMRGB(Which which = LED3);

	/**
	 * @brief Claim the three channels and set the shared PWM frequency.
	 *
	 * Called automatically by setColor()/setChannel() on first use.
	 *
	 * @param freq Frequency in Hz, shared with every other channel.
	 * @return true when all three channels were claimed successfully.
	 */
	bool begin(float freq = UNOQ_RGB_DEFAULT_FREQ);

	/** @brief True once the three channels have been claimed. */
	bool ready() const { return _begun; }

	/** @brief Set the colour, 0..255 per channel. */
	void setColor(uint8_t red, uint8_t green, uint8_t blue);

	/** @brief Set the colour from a packed 0xRRGGBB value. */
	void setColor(uint32_t rgb);

	/** @brief Set a single channel; @p channel is RED, GREEN or BLUE. */
	void setChannel(uint8_t channel, uint8_t value);

	/** @brief Switch the LED off. */
	void off() { setColor(0, 0, 0); }

	/**
	 * @brief Arduino pin number backing a colour channel.
	 * @param channel RED, GREEN or BLUE.
	 * @return The pin, or 255 when @p channel is out of range.
	 */
	uint8_t pin(uint8_t channel) const;

	/** @brief True when this LED is backed by a hardware timer channel. */
	bool isHardwarePWM() const;

	/** @brief Number of MCU-side RGB LEDs this board exposes (2 on the UNO Q). */
	static uint8_t ledCount();

	/** @brief True when the board exposes the six MCU-side LED channels. */
	static bool available();

	/** @brief The driver shared by every LED and servo instance. */
	static UNOQ_PWMServoDriver &driver();

  private:
	Which _which;
	bool _begun;
	uint8_t _pins[3];
};

#endif /* UNOQ_PWMRGB_H */
