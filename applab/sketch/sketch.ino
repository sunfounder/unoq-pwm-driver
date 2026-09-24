/*
  HardwareAnalogWrite - App Lab demo.

  Runs entirely on the STM32U585: no Python side, no bricks. The library lives
  in the board's user library folder, so a plain include is enough.

  What it does:
    * reports how many channels each engine is serving
    * fades a pin that has no timer channel, which the core cannot do
    * sweeps one servo on a timer pin and one on a software pin
*/

#include <HardwareAnalogWrite.h>
#include <HardwareServo.h>

HardwareServo servoA; /* a hardware timer pin */
HardwareServo servoB; /* a software PWM pin  */

const int PIN_A = 9; /* D9: hardware */
const int PIN_B = 4; /* D4: software */

void report() {
	Serial.println("HardwareAnalogWrite");
	Serial.print("  software tick      : ");
	Serial.print(analogWriteTickUs());
	Serial.println(" us");
	Serial.print("  software channels  : ");
	Serial.println(analogWriteSoftwareChannelCount());
	Serial.print("  D9 frequency       : ");
	Serial.print(analogWritePinFrequency(9), 1);
	Serial.println(" Hz (hardware)");
	Serial.print("  D4 frequency       : ");
	Serial.print(analogWritePinFrequency(4), 1);
	Serial.println(" Hz (software)");
}

void setup() {
	Serial.begin(115200);

	/* Bring the software engine to the servo frame rate before the servos
	   attach, so both pins share the same 50 Hz. */
	analogWriteFrequency(50.0f);

	servoA.attach(PIN_A);
	servoB.attach(PIN_B);

	report();
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

	/* Then breathe a software pin, so the engine is exercised on its own. */
	for (int value = 0; value <= ANALOG_WRITE_MAX; value += 3) {
		analogWritePin(PIN_B, value);
		delay(6);
	}
	for (int value = ANALOG_WRITE_MAX; value >= 0; value -= 3) {
		analogWritePin(PIN_B, value);
		delay(6);
	}
}
