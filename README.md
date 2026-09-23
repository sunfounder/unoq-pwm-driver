# UNOQ_PWMServoDriver

A PWM pin driver for the **Arduino UNO Q**, developed by **SunFounder**.

It lets a sketch emit PWM on the UNO Q's header pins, including the ones that are
not connected to a hardware timer, and adds a servo class and an on-board RGB
helper on top.

```cpp
#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

void setup() {
  pwm.begin(1000.0f);              // 1 kHz
  pwm.setPin(9, 2048);             // D9 -> 50 % duty
  pwm.setPin(4, 2048);             // D4 -> 50 % duty
}
```

A "channel" is an Arduino pin number, so `setPin(9, …)` drives D9. The API follows
[`Adafruit_PWMServoDriver`](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library),
and the servo class is compatible with
[`Arduino_HardwareServo`](https://github.com/arduino-libraries/Arduino_HardwareServo).

## 🔌 Compatibility

* **Arduino UNO Q** (`arduino:zephyr:unoq`)
* Arduino Zephyr core **0.54.1**, **0.90.0**, **1.0.0**
* Both `link_mode=dynamic` (default) and `link_mode=static`

## ✨ Features

* **PWM on every UNO Q header pin** — 13 on hardware timers, 9 on a software engine.
  The engine is chosen automatically and the sketch does not have to care.
* **Adafruit_PWMServoDriver-compatible API** — existing PCA9685 sketches and
  documentation stay recognisable.
* **Servo class with no limit on the number of servos** — channels are limited by
  pins, not by timers.
* **1 µs pulse resolution** on timer-backed pins.
* **Correct handling of inverted outputs** — carry on reading duty cycle as the
  fraction of the period the output is on, whatever the pin is wired to.
* **On-board RGB LEDs** — both MCU-side LEDs are dimmable from one call.
* **Diagnostics** for channels that fail to route.

## 📍 Which pins can output PWM

The UNO Q is a two-chip board: a Qualcomm QRB2210 running Linux, and an STM32U585
that runs your sketch. The board devicetree defines 70 GPIO channels, but only 22 of
them are broken out on the headers. The rest drive board-internal circuits (the LED
matrix, the JSPI and JMISC connectors, the SPI ready line, BOOT0) and are not meant
to be driven directly.

### Hardware PWM — 16 channels across 6 timers

These pins are wired to a real STM32 timer channel. The frequency is exact, the pulse
resolution is 1 µs, and generating the signal costs no CPU time.

| Timer | Pins | Channels |
|---|---|---|
| **TIM1** | D5, D11, D12, D13 | 4 |
| **TIM2** | D2, D20, D21 | 3 |
| **TIM3** | D3, D6, D8 | 3 |
| **TIM4** | D9, D10 | 2 |
| **TIM5** | LED3 red / green / blue (on-board) | 3 |
| **TIM8** | D7 | 1 |

That is **13 header pins plus the three LED3 colours**. `isHardwarePWM(pin)` reports
whether a given pin is one of them.

> The UNO Q silkscreen marks six pins with `~` (D3, D5, D6, D9, D10, D11), matching
> the UNO R3. The board actually routes 13. Trust `isHardwarePWM()`.

### Software PWM — the remaining header pins

**D0, D1, D4, A0, A1, A2, A3, A4, A5** are not connected to any timer channel, so they
are driven by a software PWM engine built on a Zephyr kernel timer.

| Pin | Why there is no timer |
|---|---|
| D0, D1 | TIM4 channels exist in silicon, but they are not routed in the board devicetree |
| D4 | PA12 has no timer output channel at all |
| A0–A5 | not routed |

The engine works at a **100 µs** resolution, so at 50 Hz a frame has 200 steps and at
1 kHz it has 10. `softwareTickUs()` reports the exact figure.

### The on-board RGB LEDs

The UNO Q carries four RGB LEDs, and they are split across the two processors. This
is the one place where "all pins" needs a caveat:

| LED | Processor | Can a sketch drive it? |
|---|---|---|
| LED1 | QRB2210 (Linux) | **No** — `arduino-app-cli` uses it as a status indicator |
| LED2 | QRB2210 (Linux) | **No** — same |
| LED3 | STM32U585 | **Yes** — three hardware PWM channels on TIM5 |
| LED4 | STM32U585 | **Yes** — no timer, so it uses the software engine |

LED3 and LED4 are therefore part of the count above, even though they are not header
pins: LED3 contributes three timer channels, LED4 three software channels. Both are
driven through `UNOQ_PWMRGB`, and because LED4 falls back to the software engine it
gets smooth dimming rather than simple on/off.

```cpp
#include <UNOQ_PWMRGB.h>

UNOQ_PWMRGB led3(UNOQ_PWMRGB::LED3);
UNOQ_PWMRGB led4(UNOQ_PWMRGB::LED4);

void setup() {
  led3.setColor(255, 0, 0);       // red
  led4.setColor(0x00FF00);        // packed 0xRRGGBB
  led3.setChannel(UNOQ_PWMRGB::BLUE, 128);   // one channel at half
  led4.off();
}
```

The LED channel numbers are read from the board devicetree rather than hard-coded, so
on the UNO Q they come out as 50/51/52 for LED3 and 53/54/55 for LED4.

## 🚀 Quick start

```cpp
#include <UNOQ_PWMServo.h>

UNOQ_PWMServo servo;

void setup() {
  servo.attach(9);                 // any pin, hardware or not
  servo.write(90);                 // degrees
}
```

## 📚 API

### Driver

| Method | Description |
|---|---|
| `begin(freq)` | Initialise the driver. Pins are claimed on first use. |
| `end()` | Release all channels and stop the software engine. |
| `setPWMFreq(float)` | Set the frequency, shared by all channels. Clamped to 1–2000 Hz. |
| `setPWM(num, on, off)` | Set a channel's duty cycle as a 12-bit window (0–4095). |
| `setPin(num, val, invert)` | Set a channel's duty value, optionally inverted. |
| `getPWM(num, off)` | Read back a channel's value. |
| `writeMicroseconds(num, us)` | Set a pulse width in microseconds. |
| `readPrescale()` | PCA9685 compatibility; derived from the real frequency. |
| `setOscillatorFrequency()`, `getOscillatorFrequency()` | PCA9685 compatibility. |
| `reset()`, `sleep()`, `wakeup()` | Stop and resume output. |

### Extensions

| Method | Description |
|---|---|
| `channelCount()` | Number of addressable channels (70). |
| `isHardwarePWM(ch)` | `true` when a hardware timer backs the channel. |
| `getPWMFreq()` | Current frequency in Hz. |
| `attach(ch)` / `detach(ch)` / `attached(ch)` | Explicit channel lifetime. |
| `softwareTickUs()` | Pulse resolution of the software engine (100 µs). |
| `softwareChannelCount()` | Channels currently on the software engine. |
| `refresh()` | No-op; provided for `SoftwareServo`-style sketches. |

### Diagnostics

| Method | Description |
|---|---|
| `lastPinmuxResult(ch)` | Result of routing the pin to its timer (0 = accepted). |
| `lastPwmResult(ch)` | Result of the last hardware PWM write (0 = ok). |
| `claimFailed(ch)` | `true` when the channel could not be claimed. |

### Servo

| Method | Description |
|---|---|
| `attach(pin, min, max)` | Attach a servo to a pin. Returns the pin number, or `INVALID_SERVO` on failure. |
| `write(deg)` | Move to an angle in degrees; values outside 0–180 are clamped. |
| `writeMicroseconds(us)` | Set the pulse width directly. |
| `read()` / `readMicroseconds()` | Last commanded angle or pulse width, or `-1` when detached. |
| `detach()` | Release the pin. |
| `attached()` | `true` while attached. |
| `driver()` | The `UNOQ_PWMServoDriver` shared by every servo instance. |

The frame is 20 ms (50 Hz) and the default pulse range is 500–2500 µs; both are
adjustable per `attach()`.

### On-board RGB

| Method | Description |
|---|---|
| `setColor(r, g, b)` | Set the colour, 0–255 per channel. |
| `setColor(0xRRGGBB)` | Set the colour from a packed value. |
| `setChannel(channel, value)` | Set a single `RED`, `GREEN` or `BLUE` channel. |
| `off()` | Switch the LED off. |
| `begin(freq)` | Claim the channels and set the shared frequency (default 1 kHz). |
| `pin(channel)` | The Arduino pin number backing a colour channel. |
| `isHardwarePWM()` | `true` for LED3, `false` for LED4. |
| `ledCount()` | Number of MCU-side RGB LEDs (2). |
| `available()` | `false` if the board exposes no `builtin-led-gpios`. |

## 🧪 Examples

| Example | Description |
|---|---|
| `PinCapabilities` | Prints which engine backs each pin. Start here. |
| `Pins0to13_1kHz` | 1 kHz / 50 % on all of D0–D13. |
| `AllPins1kHz` | 1 kHz / 50 % on every pin that can be driven. |
| `PWMPins1kHz` | 1 kHz / 50 % on every hardware PWM pin. |
| `LedRGB` | Drives the on-board LED3 and LED4 through the colour wheel. |
| `AllPinsFade` | Fades the header pins and prints the engine split. |
| `ServoSweep` | Two servos sweeping — one on D9, one on D4. |
| `HwDiagnostics` | Per-channel device, timer clock and routing results. |
| `AppLabDemo` | The sketch shipped inside the App Lab app. |

## ⚠️ Notes

* **All channels share one frequency.** Several header pins sit on the same timer, so
  a per-channel frequency is not possible.
* **All servo instances share one driver**, so attaching a servo sets the shared frame
  rate to 50 Hz. Servos and PWM at a different frequency cannot be mixed in one sketch.
* **Software PWM resolution is 100 µs.** Use a hardware PWM pin when you need finer
  resolution or a higher frequency with fine duty control.
* **`setPWM()`'s `on` offset is ignored on hardware pins.** MCU timers cannot emit an
  arbitrary phase offset.
* **Do not drive the reserved pins** for the internal SPI ready line, the analog switch
  for VREF, or BOOT0.
* **The software engine runs in interrupt context.** Interrupt load grows with the
  number of distinct pulse widths, not with the number of channels.
* `Serial` on the UNO Q is the Arduino Router's monitor. Output is only visible while
  a monitor is attached.

## 📄 License

MPL-2.0.
