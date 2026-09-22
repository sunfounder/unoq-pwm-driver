/*
    UNOQ_PWMServoDriver - pin / PWM capability tables for the Arduino UNO Q.

    These tables are derived from the board devicetree at compile time, so they
    always match the core variant that is being built against.  They intentionally
    mirror the macros used by the Arduino Zephyr core itself
    (cores/arduino/zephyrCommon.cpp) and by Arduino_HardwareServo
    (src/HardwareServo_zephyr.cpp).

    SPDX-License-Identifier: MPL-2.0
*/

#ifndef UNOQ_PWMPINS_H
#define UNOQ_PWMPINS_H

#include <Arduino.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

/* ------------------------------------------------------------------------- */
/* Pinmux backend.                                                            */
/*                                                                            */
/* The Arduino Zephyr core changed this mechanism between releases:           */
/*   * up to 0.54.1 - a single helper, _reinit_peripheral_if_needed()          */
/*   * from 0.90.0  - a dedicated pinctrl module (zephyrPinctrl.h) that can    */
/*                    apply the "arduino" pinctrl state per channel; the old  */
/*                    helper was removed entirely.                             */
/* Both are supported here so the library builds against either core.          */
/* ------------------------------------------------------------------------- */
#if defined(__has_include)
#if __has_include("zephyrPinctrl.h")
#include "zephyrPinctrl.h"
#define UNOQ_PWM_HAVE_PINCTRL_MODULE 1
#endif
#endif

#ifndef UNOQ_PWM_HAVE_PINCTRL_MODULE
#ifdef __cplusplus
extern "C" {
#endif
void _reinit_peripheral_if_needed(pin_size_t pin, const struct device *dev);
#ifdef __cplusplus
}
#endif
#endif

/* ------------------------------------------------------------------------- */
/* Total number of addressable pins, i.e. the length of digital-pin-gpios.    */
/* On the UNO Q (arduino_uno_q_stm32u585xx) this is 70.                      */
/* ------------------------------------------------------------------------- */
#define UNOQ_PWM_PIN_COUNT DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios)

/* Number of pins that have a real timer channel behind them. On the UNO Q
 * this is 16 (D2, D3, D5..D13, D20, D21 and the three RGB LED channels).     */
#define UNOQ_HW_PWM_COUNT DT_PROP_LEN(DT_PATH(zephyr_user), pwms)

/* Software PWM is only meaningful for pins that are plain GPIOs. A pin that
 * also has a timer channel is always driven by the hardware engine.          */
#define UNOQ_SW_PWM_CAPACITY (UNOQ_PWM_PIN_COUNT)

/* ------------------------------------------------------------------------- */
/* GPIO spec for every Arduino pin index (0 .. UNOQ_PWM_PIN_COUNT-1).         */
/* ------------------------------------------------------------------------- */
static const struct gpio_dt_spec unoq_gpio[UNOQ_PWM_PIN_COUNT] = {
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX,
							 (, ))};

/* ------------------------------------------------------------------------- */
/* Hardware PWM channels.                                                     */
/* ------------------------------------------------------------------------- */
#define UNOQ_PWM_DT_SPEC(n, p, i) PWM_DT_SPEC_GET_BY_IDX(n, i),
#define UNOQ_PWM_PIN_OF(n, p, i)                                                                   \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),

static const struct pwm_dt_spec unoq_hw_pwm[UNOQ_HW_PWM_COUNT] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwms, UNOQ_PWM_DT_SPEC)};

/* unoq_hw_pwm[i] drives Arduino pin unoq_hw_pwm_pin[i]. */
static const pin_size_t unoq_hw_pwm_pin[UNOQ_HW_PWM_COUNT] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwm_pin_gpios, UNOQ_PWM_PIN_OF)};

/* Some header pins are wired to an inverted timer output (the 'N' channels of
 * TIM1/TIM8, and the common-anode RGB LEDs). For those, a duty cycle that the
 * caller expresses as "time spent HIGH" has to be converted to the inverted
 * pulse width before it reaches pwm_set_dt().                                 */
#define UNOQ_PWM_FLAG_INVERTED(idx)                                                                \
	((unoq_hw_pwm[idx].flags & PWM_POLARITY_INVERTED) != 0)

/* Index of the hardware channel driving `pin`, or -1 when the pin is not on a
 * timer. Kept in a header so it can be inlined and const-folded.              */
static inline int unoq_hw_pwm_index(pin_size_t pin) {
	for (size_t i = 0; i < UNOQ_HW_PWM_COUNT; i++) {
		if (unoq_hw_pwm_pin[i] == pin) {
			return (int)i;
		}
	}
	return -1;
}

/**
 * @brief Route @p pin to the peripheral @p dev, applying the board's "arduino"
 *        pinctrl state.
 *
 * @param pin      Arduino pin number.
 * @param dev      Peripheral that should own the pin (the PWM device).
 * @param hw_index Index into unoq_hw_pwm[] for this channel.
 *
 * On cores that ship the pinctrl module the per-channel ordinal is derived with
 * the core's own helper, exactly as analogWrite() does. On older cores the
 * single legacy helper is called instead.
 * @return 0 on success, a negative errno when the core's pinctrl helper failed.
 */
static inline int unoq_pwm_apply_pinmux(pin_size_t pin, const struct device *dev,
										size_t hw_index) {
#if defined(UNOQ_PWM_HAVE_PINCTRL_MODULE)
	(void)pin;
	return zephyr::arduino::init_dev_apply_channel_pinctrl(
		dev, zephyr::arduino::state_pin_index_from_spec_index(unoq_hw_pwm, hw_index));
#else
	(void)hw_index;
	_reinit_peripheral_if_needed(pin, dev);
	return 0;
#endif
}

#endif /* UNOQ_PWMPINS_H */
