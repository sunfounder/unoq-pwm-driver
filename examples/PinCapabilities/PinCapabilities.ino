/*
  PinCapabilities - report which engine backs each UNO Q pin.

  Handy when wiring something up: it tells you whether a pin will give you an
  exact hardware-generated PWM frequency or the 100 us software fallback.
*/

#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	pwm.begin(50.0f);

	const uint16_t channels = UNOQ_PWMServoDriver::channelCount();
	uint16_t hardware = 0;

	Serial.println();
	Serial.println("UNOQ_PWMServoDriver - pin capabilities");
	Serial.print("total channels : ");
	Serial.println(channels);
	Serial.print("software tick  : ");
	Serial.print(UNOQ_PWMServoDriver::softwareTickUs());
	Serial.println(" us");

	Serial.println();
	Serial.println("channel  engine");
	Serial.println("-------  --------");
	for (uint16_t ch = 0; ch < channels; ch++) {
		const bool hw = UNOQ_PWMServoDriver::isHardwarePWM((uint8_t)ch);
		if (hw) {
			hardware++;
		}
		Serial.print(ch);
		Serial.print("\t ");
		Serial.println(hw ? "hardware" : "software");
	}

	Serial.println();
	Serial.print("hardware channels: ");
	Serial.println(hardware);
	Serial.print("software channels: ");
	Serial.println(channels - hardware);
}

void loop() {
	delay(1000);
}
