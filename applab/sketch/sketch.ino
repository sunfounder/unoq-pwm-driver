/*
  UNOQ_PWMServoDriver - App Lab demo.

  Runs entirely on the STM32U585: no Python side, no bricks. The library is
  installed in the board's user library folder, so a plain #include is enough.

  What it does:
    * prints which engine backs each header pin
    * fades every header pin up and down
    * sweeps one servo on a hardware-PWM pin and one on a software-only pin
*/

#include "UNOQ_PWMServo.h"

UNOQ_PWMServoDriver pwm;
UNOQ_PWMServo servoA; /* D9 - has a timer channel  */
UNOQ_PWMServo servoB; /* D4 - software PWM only    */

/* Pins broken out on the UNO Q headers. */
const uint8_t headerPins[] = {0, 1,  2,  3,  4,	5,	6,	7,	8,	9,	10, 11,
							  12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
const size_t headerPinCount = sizeof(headerPins) / sizeof(headerPins[0]);

void reportCapabilities() {
	uint16_t hardware = 0;
	const uint16_t channels = UNOQ_PWMServoDriver::channelCount();

	for (uint16_t ch = 0; ch < channels; ch++) {
		if (UNOQ_PWMServoDriver::isHardwarePWM((uint8_t)ch)) {
			hardware++;
		}
	}

	Serial.println("UNOQ_PWMServoDriver ready");
	Serial.print("channels        : ");
	Serial.println(channels);
	Serial.print("hardware PWM    : ");
	Serial.println(hardware);
	Serial.print("software PWM    : ");
	Serial.println(channels - hardware);
	Serial.print("software tick   : ");
	Serial.print(UNOQ_PWMServoDriver::softwareTickUs());
	Serial.println(" us");
}

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	pwm.begin(50.0f); /* 50 Hz servo frame rate */
	reportCapabilities();

	servoA.attach(9);
	servoB.attach(4);

	Serial.print("servo A on D9 (hardware): ");
	Serial.println(UNOQ_PWMServoDriver::isHardwarePWM(9) ? "yes" : "no");
	Serial.print("servo B on D4 (hardware): ");
	Serial.println(UNOQ_PWMServoDriver::isHardwarePWM(4) ? "yes" : "no");
}

void loop() {
	/* Sweep both servos in opposite directions. */
	for (int angle = 0; angle <= 180; angle += 2) {
		servoA.write(angle);
		servoB.write(180 - angle);
		delay(20);
	}
	for (int angle = 180; angle >= 0; angle -= 2) {
		servoA.write(angle);
		servoB.write(180 - angle);
		delay(20);
	}

	/* Then breathe every header pin so the software engine gets exercised too. */
	for (int value = 0; value < UNOQ_PWM_RESOLUTION; value += 64) {
		for (size_t i = 0; i < headerPinCount; i++) {
			pwm.setPin(headerPins[i], (uint16_t)value);
		}
		delay(6);
	}
	for (int value = UNOQ_PWM_RESOLUTION - 1; value >= 0; value -= 64) {
		for (size_t i = 0; i < headerPinCount; i++) {
			pwm.setPin(headerPins[i], (uint16_t)value);
		}
		delay(6);
	}
}

