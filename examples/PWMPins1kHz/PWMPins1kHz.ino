/*
  PWMPins1kHz - drive every PWM-capable pin of the UNO Q at 1000 Hz, 50 %.

  Expected signal on each pin:

      frequency : 1000 Hz
      period    : 1000 us
      high time : 500 us
      low time  : 500 us
      duty      : 50 %

  ---------------------------------------------------------------------------
  Which pins these are
  ---------------------------------------------------------------------------
  The set is "every channel backed by an STM32 timer", i.e. exactly what
  UNOQ_PWMServoDriver::isHardwarePWM() reports. On the UNO Q that is 16
  channels:

      D2  D3  D5  D6  D7  D8  D9  D10  D11  D12  D13  D20  D21
      LED3_R (ch 50)  LED3_G (ch 51)  LED3_B (ch 52)

  Thirteen of them are on the JDIGITAL header. Only six of those carry a '~'
  on the silkscreen (D3, D5, D6, D9, D10, D11) because Arduino kept the UNO R3
  markings - D2, D7, D8, D12 and D13 work as well even though they are not
  marked.

  D0, D1 and D4 are NOT in the set and are not driven:
      D0/D1  their timers are disabled in the devicetree ("not usable for
             PWM until dynamic pin muxing works"); they are USART1 / Serial
      D4     has no timer channel at all

  Because every channel here is a hardware timer channel, they all keep full
  1 us pulse resolution at 1000 Hz - no kernel-tick quantisation. The software
  engine is not used by this sketch at all.

  Nothing is driven with software PWM here, so this is also a clean way to
  confirm timer behaviour without the software engine in the picture.
*/

#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

static const float PWM_FREQ_HZ = 1000.0f;
static const uint16_t DUTY = UNOQ_PWM_RESOLUTION / 2; /* 2048 / 4096 = 50 % */

/* The on-board RGB LED (LED3, channels 50/51/52) is timer-backed as well.
 * Driving all three at once mixes the three primaries into white and changes
 * how the board looks, which is just noise while probing header pins - so it
 * is left alone by default. Set this to true to include it: it is excellent
 * visual proof that the hardware engine really is running. */
static const bool DRIVE_RGB_LEDS = false;
static const uint8_t RGB_LED_FIRST = 50;
static const uint8_t RGB_LED_LAST = 52;

/** @brief True for the channels this sketch should drive. */
static bool shouldDrive(uint16_t ch) {
	if (!UNOQ_PWMServoDriver::isHardwarePWM((uint8_t)ch)) {
		return false;
	}
	if (!DRIVE_RGB_LEDS && ch >= RGB_LED_FIRST && ch <= RGB_LED_LAST) {
		return false;
	}
	return true;
}

static uint16_t drivenCount = 0;

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	pwm.begin(PWM_FREQ_HZ);

	/* Claim and program every timer-backed channel. */
	for (uint16_t ch = 0; ch < UNOQ_PWMServoDriver::channelCount(); ch++) {
		if (!shouldDrive(ch)) {
			continue;
		}
		pwm.setPin((uint8_t)ch, DUTY);
		drivenCount++;
	}

	Serial.println();
	Serial.println("UNOQ_PWMServoDriver - PWMPins1kHz");
	Serial.print("frequency  : ");
	Serial.print(pwm.getPWMFreq());
	Serial.println(" Hz");
	Serial.print("period     : ");
	Serial.print(1000000.0f / pwm.getPWMFreq(), 1);
	Serial.println(" us");
	Serial.print("high time  : ");
	Serial.print(1000000.0f / pwm.getPWMFreq() / 2.0f, 1);
	Serial.println(" us  (50 % duty)");
	Serial.print("channels   : ");
	Serial.println(drivenCount);
	Serial.print("software   : ");
	Serial.println(pwm.softwareChannelCount());

	Serial.print("driven     :");
	for (uint16_t ch = 0; ch < UNOQ_PWMServoDriver::channelCount(); ch++) {
		if (shouldDrive(ch)) {
			Serial.print(' ');
			Serial.print(ch);
		}
	}
	Serial.println();
	Serial.println("not driven : 0,1 (Serial)  4 (no timer)  50-52 (RGB LED)  others");
	Serial.println();
}

void loop() {
	static uint32_t last = 0;
	if (millis() - last >= 5000) {
		last = millis();

		/* Count any channel the core rejected. Zero in both columns means the
		 * timer accepted the pinmux and the frequency on every channel. */
		uint16_t pinmuxErr = 0;
		uint16_t pwmErr = 0;
		for (uint16_t ch = 0; ch < UNOQ_PWMServoDriver::channelCount(); ch++) {
			if (!shouldDrive(ch)) {
				continue;
			}
			if (pwm.lastPinmuxResult((uint8_t)ch) != 0) {
				pinmuxErr++;
			}
			if (pwm.lastPwmResult((uint8_t)ch) != 0) {
				pwmErr++;
			}
		}

		Serial.print("alive ");
		Serial.print(millis() / 1000);
		Serial.print(" s, ");
		Serial.print(pwm.getPWMFreq());
		Serial.print(" Hz on ");
		Serial.print(drivenCount);
		Serial.print(" channels, errors: pinctrl=");
		Serial.print(pinmuxErr);
		Serial.print(" setpwm=");
		Serial.println(pwmErr);
	}
}
