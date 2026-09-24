/*
    HardwareAnalogWrite - analogWrite() for every pin of the Arduino UNO Q.

    The Zephyr core's analogWrite() only reaches the pins the board devicetree
    wires to an STM32 timer channel; on other pins it falls back to plain
    digital on/off. This library keeps the same "pin + value" call and adds
    what the core cannot do:

      * every GPIO gets real PWM, because non-timer pins are driven by a
        software engine built on a Zephyr k_timer,
      * the PWM frequency can be chosen at run time instead of being frozen by
        the devicetree period,
      * a pin can be released again.

    SPDX-License-Identifier: MPL-2.0
*/

#ifndef HARDWAREANALOGWRITE_H
#define HARDWAREANALOGWRITE_H

#include <Arduino.h>

#include "HardwareAnalogWritePins.h"

/** Duty values run from 0 to 255, matching analogWrite() and analogRead(). */
#define ANALOG_WRITE_MAX 255

/** Default PWM frequency, in Hz. */
#define ANALOG_WRITE_DEFAULT_FREQ 1000.0f

/** Smallest and largest frequency the engine accepts, in Hz. */
#define ANALOG_WRITE_MIN_FREQ 1.0f
#define ANALOG_WRITE_MAX_FREQ 2000.0f

/**
 * @brief analogWrite() for one pin, on whichever engine backs it.
 *
 * @param pin   Arduino pin number.
 * @param value Duty cycle, 0..255. Values outside that range are clamped.
 *
 * @return true when the pin is driving PWM, false when it is out of range or
 *         could not be claimed. Unlike the core's analogWrite() this call can
 *         be checked, which is the main reason to prefer it.
 */
bool analogWritePin(pin_size_t pin, int value);

/**
 * @brief Set the frequency used by the hardware timer behind @p pin.
 *
 * Channels that sit on the same STM32 timer share a prescaler, so a timer can
 * only run at one frequency at a time and the hardware frequency is therefore
 * global to the library. Every hardware channel that is already running is
 * re-programmed to the new frequency at the same duty cycle.
 *
 * @param pin Arduino pin number; only used to decide whether the pin is on a
 *            timer at all.
 * @param hz  Frequency in Hz, clamped to [ANALOG_WRITE_MIN_FREQ,
 *            ANALOG_WRITE_MAX_FREQ].
 * @return true when applied, false when @p pin has no timer channel (plain
 *         GPIOs run at the software frequency instead).
 */
bool analogWriteFrequency(pin_size_t pin, float hz);

/**
 * @brief Set the frequency used by every pin that is not on a timer.
 *
 * @param hz Frequency in Hz, clamped to the supported range.
 * @return The frequency actually applied.
 */
float analogWriteFrequency(float hz);

/** @brief The hardware frequency currently in use, in Hz. */
float analogWriteFrequency();

/**
 * @brief The frequency currently in use for a particular pin, in Hz.
 *
 * Lets a caller ask "what is this pin actually doing" without knowing which
 * engine backs it.
 */
float analogWritePinFrequency(pin_size_t pin);

/**
 * @brief Stop driving @p pin and release it.
 *
 * A hardware pin is left at 0 % duty; a software pin is released from the
 * engine so it stops contributing timer interrupts.
 */
void analogWriteStop(pin_size_t pin);

/** @brief True while @p pin is being driven by this library. */
bool analogWriteStarted(pin_size_t pin);

/** @brief Granularity of the software engine, in microseconds (100 on the UNO Q). */
uint32_t analogWriteTickUs();

/** @brief Number of channels currently served by the software engine. */
uint16_t analogWriteSoftwareChannelCount();

/** @brief Release every pin and stop the software engine. */
void analogWriteEnd();

/* ------------------------------------------------------------------------- */
/* Interaction with the core's analogWrite()                                  */
/*                                                                            */
/* The core ships a strong analogWrite(pin_size_t, int). This library does    */
/* NOT try to replace it: two strong definitions of one symbol cannot be      */
/* linked, and a macro would rewrite the core's own definition. Use            */
/* analogWritePin(), which is identical in spirit and reports failures.        */
/* ------------------------------------------------------------------------- */

#endif /* HARDWAREANALOGWRITE_H */
