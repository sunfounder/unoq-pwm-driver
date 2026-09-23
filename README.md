# UNOQ_PWMServoDriver

PWM output on **every** pin of the **Arduino UNO Q** — not only the handful of pins
wired to a hardware timer.

The Arduino UNO Q exposes 70 GPIOs to sketches, but only 13 header pins have a
hardware timer channel. This library gives the remaining pins a PWM output too, so
`setPWM()`, `setPin()` and `writeMicroseconds()` work on any pin you choose.

The API follows
[`Adafruit_PWMServoDriver`](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library),
and the included servo class is compatible with
[`Arduino_HardwareServo`](https://github.com/arduino-libraries/Arduino_HardwareServo).

## 🔌 Compatibility

* **Arduino UNO Q** (`arduino:zephyr:unoq`)
* Arduino Zephyr core **0.54.1**, **0.90.0**, **1.0.0**
* Both `link_mode=dynamic` (default) and `link_mode=static`

## ✨ Features

* **PWM on all 70 channels** — hardware timer where one exists, otherwise a software
  engine, selected automatically and transparently.
* **Adafruit_PWMServoDriver-compatible API** — existing PCA9685 sketches and
  documentation stay recognisable.
* **Servo class with no limit on the number of servos** — channels are limited by
  pins, not by timers.
* **1 µs pulse resolution** on timer-backed pins.
* **Correct handling of inverted outputs** — duty cycle is always expressed as time
  spent HIGH, regardless of how the pin is wired.
* **Diagnostics** for channels that fail to route.

## 🚀 Quick start

```cpp
#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

void setup() {
  pwm.begin(1000.0f);              // 1 kHz
  pwm.setPin(9, 2048);             // D9 -> 50 % duty
  pwm.setPin(4, 2048);             // D4 -> 50 % duty
}
```

```cpp
#include <UNOQ_PWMServo.h>

UNOQ_PWMServo servo;

void setup() {
  servo.attach(9);                 // any pin, hardware or not
  servo.write(90);                 // degrees
}
```

## 📍 A channel is a pin

Unlike the PCA9685 — 16 channels behind an I²C expander — **a channel number here is
an Arduino pin number**. `setPin(13, …)` drives D13. `channelCount()` is 70.

| Channels | Pins | Engine |
|---|---|---|
| 0–13 | D0 … D13 | D2, D3, D5–D13 hardware; D0, D1, D4 software |
| 14–19 | A0 … A5 | software |
| 20, 21 | D20, D21 | hardware |
| 22–24 | JSPI | software |
| 25–49 | JMISC | software |
| 50–55 | LED3, LED4 | 50, 51, 52 hardware (RGB); 53–55 software |
| 56–66 | LED matrix | software |
| 67–69 | Reserved system pins | do not drive |

`isHardwarePWM()` reports which engine backs a pin, and the `PinCapabilities` example
prints the whole map.

> The UNO Q silkscreen marks six pins with `~` (D3, D5, D6, D9, D10, D11), matching the
> UNO R3. The board actually routes 13. Trust `isHardwarePWM()`.

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

## 🧪 Examples

| Example | Description |
|---|---|
| `Pins0to13_1kHz` | 1 kHz / 50 % on all of D0–D13. |
| `AllPins1kHz` | 1 kHz / 50 % on every pin that can be driven. |
| `PWMPins1kHz` | 1 kHz / 50 % on every hardware PWM pin. |
| `PinCapabilities` | Prints which engine backs each of the 70 channels. |
| `AllPinsFade` | Fades the header pins and prints the engine split. |
| `ServoSweep` | Two servos sweeping — one on D9, one on D4. |
| `HwDiagnostics` | Per-channel device, timer clock and routing results. |
| `AppLabDemo` | The sketch shipped inside the App Lab app. |

## 📦 Installation

### Arduino IDE / arduino-cli

Copy the library into your sketchbook libraries folder:

```
~/Arduino/libraries/UNOQ_PWMServoDriver/       # on the board
%USERPROFILE%\Documents\Arduino\libraries\     # on Windows
```

### Arduino App Lab

An App Lab `sketch/` folder must keep its `sketch.yaml`, and its presence switches
`arduino-cli` into profile mode, which does not scan `~/Arduino/libraries`. The App
therefore carries the library sources inside its own sketch folder and includes them
with quotes:

```cpp
#include "UNOQ_PWMServoDriver.h"
```

`deploy.ps1` performs the copy and uploads both the library and the app:

```powershell
.\deploy.ps1                 # default target: arduino@192.168.18.119
.\deploy.ps1 -Device arduino@other-host
```

Start the app with App Lab's **Run** button. `arduino-app-cli app restart` conflicts
with App Lab ownership of the app.

## ⚠️ Notes

* **All channels share one frequency.** Several header pins sit on the same timer, so
  a per-channel frequency is not possible.
* **All servo instances share one driver**, so attaching a servo sets the shared frame
  rate to 50 Hz. Servos and PWM at a different frequency cannot be mixed in one sketch.
* **Software PWM resolution is 100 µs.** At 50 Hz that is 200 steps per frame; at
  1 kHz it is 10. Use a hardware PWM pin when you need finer resolution.
* **`setPWM()`'s `on` offset is ignored on hardware pins.** MCU timers cannot emit an
  arbitrary phase offset.
* **Pins 67–69 are reserved** for system functions (internal SPI ready, analog switch
  for VREF, BOOT0). Do not drive them.
* **The software engine runs in interrupt context.** Interrupt load grows with the
  number of distinct pulse widths, not with the number of channels.
* `Serial` on the UNO Q is the Arduino Router's monitor. Output is only visible while
  a monitor is attached.

## 📄 License

MPL-2.0, matching the Arduino libraries this work derives its interfaces from.
