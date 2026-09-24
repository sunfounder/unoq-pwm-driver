/*
  ServoSweep

  Sweeps two servos in opposite directions: one on a pin with a hardware timer
  channel, one on a pin with no timer at all. Both use the same API, which is
  the point of the library - the sketch never has to know which engine backs
  which pin.

  Circuit: servo signal wires to pin 9 and pin 4, plus power and ground.
  Servos draw a lot of current, so use a separate 5 V supply rather than the
  board's regulator.

  This example code is in the public domain.
*/

#include <HardwareServo.h>

HardwareServo servoA; /* on a hardware timer pin  */
HardwareServo servoB; /* on a software PWM pin    */

const int PIN_A = 9; /* D9 has a timer channel */
const int PIN_B = 4; /* D4 has none            */

void setup() {
	Serial.begin(115200);

	servoA.attach(PIN_A);
	servoB.attach(PIN_B);

	Serial.println("ServoSweep");
	Serial.print("pin ");
	Serial.print(PIN_A);
	Serial.print(": ");
	Serial.println(analogWritePinFrequency(PIN_A), 1);
	Serial.print("pin ");
	Serial.print(PIN_B);
	Serial.print(": ");
	Serial.println(analogWritePinFrequency(PIN_B), 1);
}

void loop() {
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
}
