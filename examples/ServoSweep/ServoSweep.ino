/*
  ServoSweep - two servos sweeping in opposite directions.

  Servo A sits on D9, which has a hardware timer channel.
  Servo B sits on D4, which has no timer at all and is therefore driven by the
  software engine.  Both are commanded through the very same API.

  Unlike the classic Servo library there is no cap on the number of attached
  servos, because a pin is only ever limited by the engine that backs it.
*/

#include <UNOQ_PWMServo.h>

UNOQ_PWMServo servoA;
UNOQ_PWMServo servoB;

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	servoA.attach(9);
	servoB.attach(4);

	Serial.println("UNOQ_PWMServo - ServoSweep");
	Serial.print("D9 hardware PWM: ");
	Serial.println(UNOQ_PWMServoDriver::isHardwarePWM(9) ? "yes" : "no");
	Serial.print("D4 hardware PWM: ");
	Serial.println(UNOQ_PWMServoDriver::isHardwarePWM(4) ? "yes" : "no");
}

void loop() {
	for (int angle = 0; angle <= 180; angle++) {
		servoA.write(angle);
		servoB.write(180 - angle);
		delay(15);
	}
	for (int angle = 180; angle >= 0; angle--) {
		servoA.write(angle);
		servoB.write(180 - angle);
		delay(15);
	}
}
