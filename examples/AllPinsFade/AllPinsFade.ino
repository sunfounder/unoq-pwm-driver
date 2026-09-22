/*
  AllPinsFade - breathe PWM on every header pin of the UNO Q.

  Pins that are wired to a timer (D2, D3, D5..D13, D20, D21) are driven by the
  hardware engine; every other pin, including D0, D1, D4 and the analog pins,
  is driven by the software engine.  The sketch does not have to care which is
  which.

  Open the Serial Monitor at 115200 baud to see the engine split.
*/

#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

/* Every pin broken out on the UNO Q headers, except D0 and D1.
 *
 * D0/D1 are USART1, which is the very port this sketch prints on. Driving them
 * as PWM would take the pin away from the UART and the output would go silent,
 * so they are deliberately skipped. See the header list in the README. */
const uint8_t headerPins[] = {
	2,  3,	4,  5,	6,  7,	8,  9,	10, 11, 12, 13, /* D2..D13 (D0/D1 = Serial) */
	14, 15, 16, 17, 18, 19,						   /* A0..A5  */
	20, 21										   /* D20/D21 */
};
const size_t headerPinCount = sizeof(headerPins) / sizeof(headerPins[0]);

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	/* 500 Hz keeps the software engine comfortable and is plenty for LEDs. */
	pwm.begin(500.0f);

	Serial.println("UNOQ_PWMServoDriver - AllPinsFade");
	Serial.print("channels: ");
	Serial.println(UNOQ_PWMServoDriver::channelCount());
	Serial.print("software tick: ");
	Serial.print(UNOQ_PWMServoDriver::softwareTickUs());
	Serial.println(" us");

	Serial.print("hardware PWM pins:");
	for (size_t i = 0; i < headerPinCount; i++) {
		if (UNOQ_PWMServoDriver::isHardwarePWM(headerPins[i])) {
			Serial.print(' ');
			Serial.print(headerPins[i]);
		}
	}
	Serial.println();

	Serial.print("software PWM pins:");
	for (size_t i = 0; i < headerPinCount; i++) {
		if (!UNOQ_PWMServoDriver::isHardwarePWM(headerPins[i])) {
			Serial.print(' ');
			Serial.print(headerPins[i]);
		}
	}
	Serial.println();
}

void loop() {
	for (int value = 0; value < UNOQ_PWM_RESOLUTION; value += 32) {
		for (size_t i = 0; i < headerPinCount; i++) {
			pwm.setPin(headerPins[i], (uint16_t)value);
		}
		delay(8);
	}
	for (int value = UNOQ_PWM_RESOLUTION - 1; value >= 0; value -= 32) {
		for (size_t i = 0; i < headerPinCount; i++) {
			pwm.setPin(headerPins[i], (uint16_t)value);
		}
		delay(8);
	}
}
