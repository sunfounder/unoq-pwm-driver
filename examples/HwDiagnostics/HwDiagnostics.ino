/*
  HwDiagnostics - find out why a hardware PWM channel produces no output.

  The driver used to discard the results of init_dev_apply_channel_pinctrl()
  and pwm_set_dt(), so a channel that failed to route to its timer looked
  exactly like a dead pin while the sketch happily reported success. This
  sketch prints every intermediate value instead.

  Expected output, per timer-backed channel:

      idx pin dev    rdy ch  cps       dtper  pinctrl  setpwm
      0   2   pwm2   yes 2   16000000  2000000  0       0

  What each column means:
    rdy      - pwm_is_ready_dt(): is the PWM device initialised at all
    cps      - pwm_get_cycles_per_sec(): timer clock, i.e. the pulse
               resolution. 1000 Hz needs cps >= 1000 just to exist, and
               cps/1000 <= 65535 (16-bit ARR) to be representable exactly.
    dtper    - the period from devicetree (ns), i.e. PWM_HZ(500) -> 2000000
    pinctrl  - return of init_dev_apply_channel_pinctrl(): 0 = routed ok
    setpwm   - return of pwm_set_dt(): 0 = accepted

  Anything non-zero in the last two columns is the bug.
*/

#include <zephyr/drivers/pwm.h>

#include "UNOQ_PWMServoDriver.h"

UNOQ_PWMServoDriver pwm;

static const float PWM_FREQ_HZ = 1000.0f;
static const uint16_t DUTY = UNOQ_PWM_RESOLUTION / 2;

static void printHeader() {
	Serial.println();
	Serial.println("=== HW PWM diagnostics ===");
	Serial.println("idx pin dev   rdy ch  cps       dtper    sidx pinctrl setpwm");
}

static void report(uint8_t ch) {
	const int idx = unoq_hw_pwm_index((pin_size_t)ch);
	if (idx < 0) {
		return;
	}
	const struct pwm_dt_spec *s = &unoq_hw_pwm[idx];

	uint64_t cps = 0;
	(void)pwm_get_cycles_per_sec(s->dev, s->channel, &cps);

	Serial.print(idx);
	Serial.print("\t");
	Serial.print(unoq_hw_pwm_pin[idx]);
	Serial.print("   ");
	Serial.print(s->dev->name);
	Serial.print("\t");
	Serial.print(pwm_is_ready_dt(s) ? "yes" : "no ");
	Serial.print("\t");
	Serial.print(s->channel);
	Serial.print("   ");
	Serial.print((uint32_t)cps);
	Serial.print("\t");
	Serial.print(s->period);
	Serial.print("\t");
#if defined(UNOQ_PWM_HAVE_PINCTRL_MODULE)
	/* Which pin inside the device's "arduino" pinctrl state this channel is
	 * supposed to select. If this ordinal is wrong the core happily routes a
	 * different pin and the intended one stays silent. */
	Serial.print(zephyr::arduino::state_pin_index_from_spec_index(unoq_hw_pwm, (size_t)idx));
#else
	Serial.print("-");
#endif
	Serial.print("\t");
	Serial.print(pwm.lastPinmuxResult(ch));
	Serial.print("\t");
	Serial.println(pwm.lastPwmResult(ch));
}

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		;
	}

	printHeader();

	pwm.begin(PWM_FREQ_HZ);

	/* Claim and program every timer-backed channel, then report. */
	for (uint16_t ch = 0; ch < UNOQ_PWMServoDriver::channelCount(); ch++) {
		if (!UNOQ_PWMServoDriver::isHardwarePWM((uint8_t)ch)) {
			continue;
		}
		pwm.setPin((uint8_t)ch, DUTY);
	}
	for (uint16_t ch = 0; ch < UNOQ_PWMServoDriver::channelCount(); ch++) {
		if (!UNOQ_PWMServoDriver::isHardwarePWM((uint8_t)ch)) {
			continue;
		}
		report((uint8_t)ch);
	}

	Serial.println();
	Serial.print("requested: ");
	Serial.print(pwm.getPWMFreq());
	Serial.print(" Hz, duty ");
	Serial.print(DUTY);
	Serial.print("/");
	Serial.println(UNOQ_PWM_RESOLUTION);
	Serial.println("legend: rdy=pwm_is_ready_dt cps=cycles/sec dtper=dt period ns");
	Serial.println("        pinctrl=init_dev_apply_channel_pinctrl rc setpwm=pwm_set_dt rc");
	Serial.println("        0 in the last two columns means the core accepted it.");
	Serial.println();
}

void loop() {
	static uint32_t last = 0;
	if (millis() - last >= 5000) {
		last = millis();
		Serial.print("alive ");
		Serial.print(millis() / 1000);
		Serial.println(" s");
	}
}
