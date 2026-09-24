/*
  Fade

  Ramps a PWM pin up and down, like the classic Arduino Fade example.

  The difference from the built-in analogWrite() is the pin it runs on: D4 has
  no timer channel on the UNO Q, so the core falls back to plain digital
  on/off there. This library drives it with a software PWM engine instead, so
  the LED really does fade.

  Circuit: an LED and a resistor from pin 4 to GND, or just watch the pin with
  a scope. Change FADE_PIN to any pin of the board.

  This example code is in the public domain.
*/

#include <HardwareAnalogWrite.h>

const int FADE_PIN = 4; /* D4: no timer channel, so software PWM */

void setup() {
	/* 1 kHz is smooth for an LED. The default is already 1000 Hz, this call
	   just makes the intent visible, and shows that the frequency is a choice
	   rather than something the devicetree decides. */
	analogWriteFrequency(1000.0f);
}

void loop() {
	/* Fade up */
	for (int value = 0; value <= ANALOG_WRITE_MAX; value += 5) {
		analogWritePin(FADE_PIN, value);
		delay(10);
	}

	/* Fade down */
	for (int value = ANALOG_WRITE_MAX; value >= 0; value -= 5) {
		analogWritePin(FADE_PIN, value);
		delay(10);
	}
}
