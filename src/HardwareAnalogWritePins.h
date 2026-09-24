/*
    HardwareAnalogWrite - pin and PWM capability tables for the Arduino UNO Q.

    The tables are derived from the board devicetree at compile time, so they
    always match the core variant being built against. They intentionally mirror
    the macros the Arduino Zephyr core uses itself in
    cores/arduino/wiring_analog.cpp and cores/arduino/zephyrPinctrl.h.

    SPDX-License-Identifier: MPL-2.0
*/

#ifndef HARDWAREANALOGWRITEPINS_H
#define HARDWAREANALOGWRITEPINS_H

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
/*                    apply the "arduino" pinctrl state per channel           */
/* Both are supported so the library builds against either core.              */
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

/* Total number of addressable pins, i.e. the length of digital-pin-gpios.
 * 70 on the UNO Q (arduino_uno_q_stm32u585xx). */
#define UNOQ_PWM_PIN_COUNT DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios)

/* Number of pins backed by a real timer channel. 16 on the UNO Q. */
#define UNOQ_HW_PWM_COUNT DT_PROP_LEN(DT_PATH(zephyr_user), pwms)

/* GPIO spec for every Arduino pin index (0 .. UNOQ_PWM_PIN_COUNT-1). */
static const struct gpio_dt_spec unoq_gpio[UNOQ_PWM_PIN_COUNT] = {
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX,
							 (, ))};

/* DT_FOREACH_PROP_ELEM passes three arguments, PWM_DT_SPEC_GET_BY_IDX takes
 * two, so the call has to be wrapped. */
#define UNOQ_PWM_DT_SPEC(n, p, i) PWM_DT_SPEC_GET_BY_IDX(n, i),

static const struct pwm_dt_spec unoq_hw_pwm[UNOQ_HW_PWM_COUNT] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwms, UNOQ_PWM_DT_SPEC)};

/* unoq_hw_pwm[i] drives Arduino pin unoq_hw_pwm_pin[i]. */
#define UNOQ_PWM_PIN_OF(n, p, i)                                                                   \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),

static const pin_size_t unoq_hw_pwm_pin[UNOQ_HW_PWM_COUNT] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwm_pin_gpios, UNOQ_PWM_PIN_OF)};

/* Some channels are wired to an inverted timer output (the 'N' channels and
 * the common-anode RGB LEDs). */
static inline bool unoq_hw_pwm_inverted(size_t idx) {
	return (unoq_hw_pwm[idx].flags & PWM_POLARITY_INVERTED) != 0;
}

/**
 * @brief Index of the hardware channel driving @p pin, or -1 when the pin is
 *        not on a timer. Kept in the header so it inlines and const-folds.
 */
static inline int unoq_hw_pwm_index(pin_size_t pin) {
	for (size_t i = 0; i < UNOQ_HW_PWM_COUNT; i++) {
		if (unoq_hw_pwm_pin[i] == pin) {
			return (int)i;
		}
	}
	return -1;
}

/**
 * @brief Route @p pin to @p dev, applying the board's "arduino" pinctrl state.
 *
 * @param pin      Arduino pin number.
 * @param dev      Peripheral that should own the pin (the PWM device).
 * @param hw_index Index into unoq_hw_pwm[] for this channel.
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

#endif /* HARDWAREANALOGWRITEPINS_H */
