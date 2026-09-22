# UNOQ_PWMServoDriver

Full-pin PWM / servo driver for the **Arduino UNO Q** (STM32U585, Arduino Zephyr core).

The UNO Q exposes **70 GPIOs** to sketches, but only **16** of them are wired to an
STM32 timer channel. This library makes **every pin able to emit PWM**, with a public
API modelled on [`Adafruit_PWMServoDriver`](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library)
and a servo class compatible with [`Arduino_HardwareServo`](https://github.com/arduino-libraries/Arduino_HardwareServo).

## How it works

Two engines cooperate behind a single API:

| Engine | Pins | Frequency | Pulse resolution | CPU cost |
|---|---|---|---|---|
| **Hardware** — `pwm_set_dt()` on the board devicetree `pwms` channels | D2, D3, D5–D13, D20, D21 + RGB LED R/G/B | any 1–2000 Hz | 1 µs | none |
| **Software** — one Zephyr `k_timer` in one-shot mode, falling edges scheduled per distinct pulse width | **every remaining GPIO**, including D0, D1, D4 and A0–A5 | any 1–2000 Hz | 100 µs (1 kernel tick) | ~2 interrupts per channel per period |

The software engine exploits the fact that all channels rise together at the start of a
period, so only the *distinct* falling edges need scheduling: a servo frame at 50 Hz costs
one timer re-arm per attached pin, not a high-rate tick.

Pins whose timer output is inverted in the devicetree (`TIM1_CHxN`, `TIM8_CH4N`, and the
common-anode RGB LEDs) are handled transparently — you always express duty as *time spent
HIGH*.

## API

```cpp
#include <UNOQ_PWMServoDriver.h>

UNOQ_PWMServoDriver pwm;

void setup() {
  pwm.begin(50.0f);          // 50 Hz frame rate
  pwm.setPin(9, 2048);       // D9  -> 50 % duty
  pwm.setPWM(4, 0, 1024);    // D4  -> 25 % duty, PCA9685 style
  pwm.writeMicroseconds(7, 1500);  // D7 -> 1.5 ms pulse
}
```

### Adafruit-compatible surface

| Method | Notes |
|---|---|
| `begin(freq)` | Initialise; pins are claimed lazily. |
| `setPWMFreq(float)` | Shared by all channels, exactly like the PCA9685's single oscillator. |
| `setPWM(num, on, off)` | 12-bit window. Only the *width* is reproduced — MCU timers have no phase offset. |
| `setPin(num, val, invert)` | PCA9685 helper. |
| `getPWM(num, off)` | Reads back the shadow registers. |
| `writeMicroseconds(num, us)` | Servo-style entry point. |
| `readPrescale()`, `setOscillatorFrequency()` | Compatibility shims, derived from the real frequency. |
| `reset()`, `sleep()`, `wakeup()` | Sleep drives every claimed channel to 0. |

### Extensions

| Method | Notes |
|---|---|
| `channelCount()` | Number of addressable channels (== pin count, 70 on the UNO Q). |
| `isHardwarePWM(ch)` | True when a real timer channel backs the pin. |
| `attach(ch)` / `detach(ch)` / `attached(ch)` | Explicit channel lifetime. |
| `softwareTickUs()` | Software engine granularity (100 µs here). |
| `softwareChannelCount()` | How many channels are currently on the software engine. |
| `refresh()` | No-op; the engine is interrupt driven. Kept for `SoftwareServo`-style sketches. |

### Servo class

```cpp
#include <UNOQ_PWMServo.h>

UNOQ_PWMServo servo;
servo.attach(4);          // any pin works, hardware or not
servo.write(90);          // degrees
servo.writeMicroseconds(1500);
int us = servo.readMicroseconds();
servo.detach();
```

There is **no limit on the number of servos**, unlike timer-bound Servo implementations:
channels are limited by pins, not by timers.

## A channel is a pin

Unlike the PCA9685 (16 channels on an I²C expander), a channel number here **is the
Arduino pin number**. `setPin(13, …)` drives D13.

## Known limits

* **Software PWM resolution is one kernel tick**, measured at compile time via
  `CONFIG_SYS_CLOCK_TICKS_PER_SEC` (10000 on the UNO Q ⇒ 100 µs). At 50 Hz that is
  200 steps per frame ≈ 0.9° on a 180° servo. Use a hardware-PWM pin when you need
  finer servo positioning or a frequency above a few hundred Hz with many channels.
* `setPWM()`'s `on` phase offset is ignored on hardware channels.
* All channels share one frequency, because several header pins sit on the same timer.
* The software engine runs in interrupt context; with a large number of claimed software
  channels at a high frequency, interrupt load grows accordingly.

## Compatibility

* Board: `arduino:zephyr:unoq` — Arduino UNO Q
* Core: Arduino Zephyr core (tested against `arduino:zephyr` 0.54.1)
* Both `link_mode=dynamic` (default) and `link_mode=static` build cleanly.

## Installing

The canonical library lives in `src/` and is a plain Arduino library — drop it
into the user library folder for general use:

```
~/Arduino/libraries/UNOQ_PWMServoDriver/      # on the board
%USERPROFILE%\Documents\Arduino\libraries\    # on Windows
```

> **App Lab gotcha.** An App Lab `sketch/` folder must keep its `sketch.yaml`,
> because App Lab only recognises apps that carry one — *and* its presence
> switches `arduino-cli` into **profile mode**, which resolves libraries only
> from the profile's `libraries:` list and does **not** scan
> `~/Arduino/libraries`. A named local library cannot be pinned in the profile
> either (`Library '…' not found`), so the App carries the library sources
> inside its own sketch folder, where Arduino compiles them and puts them on the
> include path. `deploy.ps1` performs that copy; the duplicated files are
> git-ignored and `src/` stays the single source of truth.

One consequence: inside the App Lab sketch the include uses quotes
(`#include "UNOQ_PWMServo.h"`), because angle-bracket lookups only search the
library directories.

## Examples

* `AllPinsFade` — breathes PWM across every header pin and prints the engine split.
* `ServoSweep` — two servos, one on a hardware pin, one on a software pin.
* `PinCapabilities` — prints which engine backs each of the 70 channels.

## Licence

MPL-2.0, matching the Arduino libraries this work derives its interfaces from.
