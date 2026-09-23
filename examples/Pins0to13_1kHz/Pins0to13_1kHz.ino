/*
  Pins0to13_1kHz - 1000 Hz / 50 % on ALL of D0..D13, no exceptions.

  The previous test filtered on isHardwarePWM(), which silently dropped D0, D1
  and D4. They cannot do *hardware* PWM, but the library's software engine can
  drive them perfectly well - a pin only has to be a GPIO for that. This sketch
  drives every pin of the digital header so the scope can confirm it:

      D2 D3 D5 D6 D7 D8 D9 D10 D11 D12 D13   -> hardware engine, 1 us
      D0 D1 D4                                -> software engine, 100 us

  ---------------------------------------------------------------------------
  The D0/D1 question
  ---------------------------------------------------------------------------
  In the silicon D0/D1 are USART1_RX/TX, and their TIM4_CH1/CH2 channels exist,
  but the board devicetree comments both of those pwms entries out, so the core
  refuses to route them to a timer. Driving them as plain GPIO is still
  allowed, which is exactly what the software engine does.

  Whether that costs you the console is the interesting part, and this sketch
  answers it from the inside: the report below is printed FIRST, then D0/D1 are
  claimed. If the heartbeat keeps coming, USART1 is not the console on this
  core (the sketch console is the router Monitor, which runs over LPUART1 per
  "arduino,router-serial = <&lpuart1>"). If the heartbeat stops right after the
  report, D0/D1 really are the console and must be left alone.

  Either way the scope will show a 1 kHz square wave on all fourteen pins.
*/

#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

static const float PWM_FREQ_HZ = 1000.0f;
static const uint16_t DUTY = UNOQ_PWM_RESOLUTION / 2; /* 50 % */
static const uint8_t LAST_PIN = 13;

static void reportChannel(uint16_t ch) {
	Serial.print("  D");
	Serial.print(ch);
	Serial.print("\t");
	Serial.print(UNOQ_PWMServoDriver::isHardwarePWM((uint8_t)ch) ? "hardware" : "software");
	Serial.print("\tpinctrl=");
	Serial.print(pwm.lastPinmuxResult((uint8_t)ch));
	Serial.print("\tsetpwm=");
	Serial.print(pwm.lastPwmResult((uint8_t)ch));
	Serial.print("\tclaim=");
	Serial.println(pwm.claimFailed((uint8_t)ch) ? "FAILED" : "ok");
}

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	/* Start at 1000 Hz, then drive every pin of the digital header. */
	pwm.begin(PWM_FREQ_HZ);

	for (uint16_t ch = 0; ch <= LAST_PIN; ch++) {
		pwm.setPin((uint8_t)ch, DUTY);
	}

	Serial.println();
	Serial.println("UNOQ_PWMServoDriver - Pins0to13_1kHz (all of D0..D13)");
	Serial.print("frequency : ");
	Serial.print(pwm.getPWMFreq());
	Serial.println(" Hz, 50 % duty, 500 us high / 500 us low");
	Serial.print("channels  : ");
	Serial.print(LAST_PIN + 1);
	Serial.print("  (hardware ");
	Serial.print(LAST_PIN + 1 - pwm.softwareChannelCount());
	Serial.print(", software ");
	Serial.print(pwm.softwareChannelCount());
	Serial.println(")");
	Serial.println();
	Serial.println("pin\tengine\t\tdiagnostics");
	for (uint16_t ch = 0; ch <= LAST_PIN; ch++) {
		reportChannel(ch);
	}
	Serial.println();
	Serial.println("D0/D1 claimed above. If the heartbeat below keeps printing,");
	Serial.println("USART1 is NOT the console on this core and D0/D1 are free.");
	Serial.println();
}

void loop() {
	static uint32_t last = 0;
	if (millis() - last >= 5000) {
		last = millis();
		Serial.print("alive ");
		Serial.print(millis() / 1000);
		Serial.print(" s, ");
		Serial.print(pwm.getPWMFreq());
		Serial.print(" Hz on ");
		Serial.print(LAST_PIN + 1);
		Serial.print(" pins, software engine busy with ");
		Serial.print(pwm.softwareChannelCount());
		Serial.println(" channels");
	}
}
