/*
  LedRGB - drive the on-board RGB LEDs of the Arduino UNO Q.

  The UNO Q has four RGB LEDs, but only two of them belong to the sketch:

      LED1, LED2   Linux side (QRB2210). Used by arduino-app-cli as app status
                   indicators; a sketch cannot reach them.
      LED3         STM32 side, PH10/PH11/PH12 - hardware PWM.
      LED4         STM32 side, PH13/PH14/PH15 - software PWM, still dimmable.

  Both LED3 and LED4 are driven through the same API here. The channel numbers
  are taken from the board devicetree, so nothing is hard-coded.
*/

#include <UNOQ_PWMRGB.h>

UNOQ_PWMRGB led3(UNOQ_PWMRGB::LED3);
UNOQ_PWMRGB led4(UNOQ_PWMRGB::LED4);

void report(const char *name, UNOQ_PWMRGB &led) {
	Serial.print(name);
	Serial.print(": R=ch");
	Serial.print(led.pin(UNOQ_PWMRGB::RED));
	Serial.print(" G=ch");
	Serial.print(led.pin(UNOQ_PWMRGB::GREEN));
	Serial.print(" B=ch");
	Serial.print(led.pin(UNOQ_PWMRGB::BLUE));
	Serial.print("  engine=");
	Serial.println(led.isHardwarePWM() ? "hardware" : "software");
}

void setup() {
	Serial.begin(115200);
	delay(1500);

	Serial.println();
	Serial.println("LedRGB - on-board RGB LEDs");
	Serial.println("--------------------------");

	if (!UNOQ_PWMRGB::available()) {
		Serial.println("This board does not expose builtin-led-gpios.");
		return;
	}

	Serial.print("LEDs available: ");
	Serial.println(UNOQ_PWMRGB::ledCount());

	led3.begin(1000.0f);
	led4.begin(1000.0f);

	report("LED3", led3);
	report("LED4", led4);

	/* Named colours first, 0xRRGGBB. */
	const uint32_t colours[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000};
	for (uint8_t i = 0; i < 5; i++) {
		led3.setColor(colours[i]);
		led4.setColor(colours[i]);
		delay(700);
	}
}

void loop() {
	/* Fade both LEDs through the hue circle, offset from each other. */
	static uint8_t step = 0;

	led3.setColor(wheel(step));
	led4.setColor(wheel((uint8_t)(step + 128)));

	/* Brightness ramp on top, so dimming is visible too. */
	const uint8_t level = (uint8_t)((step < 128) ? step * 2 : (255 - step) * 2);
	led3.setChannel(UNOQ_PWMRGB::GREEN, level);

	step++;
	delay(25);
}

/* 0..255 -> a fully saturated colour, the classic three-segment wheel. */
uint32_t wheel(uint8_t pos) {
	if (pos < 85) {
		return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(pos * 3) << 8);
	}
	if (pos < 170) {
		pos = (uint8_t)(pos - 85);
		return ((uint32_t)(pos * 3) << 8) | (uint32_t)(255 - pos * 3);
	}
	pos = (uint8_t)(pos - 170);
	return ((uint32_t)(pos * 3) << 16) | (uint32_t)(255 - pos * 3);
}
