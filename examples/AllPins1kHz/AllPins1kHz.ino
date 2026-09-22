/*
  AllPins1kHz - drive every pin we can safely drive at 1000 Hz, 50 % duty.

  Purpose: a signal you can verify with a scope or logic analyser on every
  exposed pin at once.

    frequency : 1000 Hz
    period    : 1000 us
    duty      : 50 %
    high time : 500 us

  ---------------------------------------------------------------------------
  Pins that are deliberately NOT driven
  ---------------------------------------------------------------------------
  D0 / D1  (channels 0, 1)
      They are USART1 - the port this sketch prints its report on, and the
      port the Arduino Router uses. Driving them with PWM silences the sketch
      and breaks the Bridge to the Linux side.

  channel 67  (PG13)
      Internal SPI ready line towards the Linux side.

  channel 68  (PA2)
      Analog switch that selects VREF.

  channel 69  (PH3)
      BOOT0 strap - driving it can affect boot.

  Channels 2..66 are safe to drive in isolation, but note what else sits on
  some of them: D10-D13 are SPI2, D20/D21 are I2C2 and 50/51/52 are the RGB
  LED channels. Nothing else is using those buses in this sketch, so driving
  them is fine here.

  ---------------------------------------------------------------------------
  Resolution caveat at 1 kHz
  ---------------------------------------------------------------------------
  The software engine advances one kernel tick at a time, which on the UNO Q
  is 100 us. At 1000 Hz the whole period is only 10 ticks, so a software
  channel has 10 duty steps total. 50 % is exactly 5 ticks and therefore
  exact; other duty values will quantise. The 16 hardware channels keep their
  1 us resolution at any frequency.
*/

#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

static const float PWM_FREQ_HZ = 1000.0f;
static const uint16_t DUTY = UNOQ_PWM_RESOLUTION / 2; /* 2048 / 4096 = 50 % */

/** @brief True for channels that are safe to drive in this test. */
static bool isDrivable(uint16_t ch) {
	if (ch <= 1) {
		return false; /* D0/D1 = USART1 = Serial */
	}
	if (ch >= 67) {
		return false; /* PG13 / PA2 / PH3: internal system pins */
	}
	return ch < UNOQ_PWMServoDriver::channelCount();
}

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	pwm.begin(PWM_FREQ_HZ);

	uint16_t driven = 0;
	uint16_t hardware = 0;

	for (uint16_t ch = 0; ch < UNOQ_PWMServoDriver::channelCount(); ch++) {
		if (!isDrivable(ch)) {
			continue;
		}
		pwm.setPin((uint8_t)ch, DUTY);
		driven++;
		if (UNOQ_PWMServoDriver::isHardwarePWM((uint8_t)ch)) {
			hardware++;
		}
	}

	Serial.println();
	Serial.println("UNOQ_PWMServoDriver - AllPins1kHz");
	Serial.print("frequency  : ");
	Serial.print(pwm.getPWMFreq());
	Serial.println(" Hz");
	Serial.print("period     : ");
	Serial.print(1000000.0f / pwm.getPWMFreq());
	Serial.println(" us");
	Serial.print("high time  : ");
	Serial.print(1000000.0f / pwm.getPWMFreq() / 2.0f);
	Serial.println(" us  (50 % duty)");
	Serial.print("channels   : ");
	Serial.print(driven);
	Serial.print(" driven  (hardware ");
	Serial.print(hardware);
	Serial.print(", software ");
	Serial.print(driven - hardware);
	Serial.println(")");
	Serial.println("skipped    : 0,1 (Serial)  67 (SPI RDY)  68 (VREF)  69 (BOOT0)");
	Serial.println();
}

void loop() {
	/* Heartbeat: also proves the sketch is alive while every pin is toggling
	 * at 1 kHz. Monitor output only reaches a client that is already
	 * listening, so it has to be repeated. */
	static uint32_t last = 0;
	if (millis() - last >= 5000) {
		last = millis();
		Serial.print("alive, uptime ");
		Serial.print(millis() / 1000);
		Serial.print(" s, ");
		Serial.print(pwm.getPWMFreq());
		Serial.print(" Hz, software channels ");
		Serial.println(pwm.softwareChannelCount());
	}
}
