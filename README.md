# HardwareAnalogWrite

`analogWrite()` for **every pin** of the Arduino UNO Q, by **SunFounder**.

```cpp
#include <HardwareAnalogWrite.h>

void setup() {
  analogWritePin(4, 128);              // 50 % on D4
  analogWriteFrequency(1000.0f);       // 1 kHz on the software engine
}

void loop() {}
```

`analogWritePin(pin, value)` takes an Arduino pin and a duty value from 0 to 255,
exactly like `analogWrite()`. The difference is where it works.

## Why this library exists

The Zephyr core already ships an `analogWrite()`. It has three limitations that
this library removes:

| | core `analogWrite()` | `analogWritePin()` |
|---|---|---|
| Pins without a timer (D0, D1, D4, A0–A5) | plain digital on/off | **real PWM** from a software engine |
| Frequency | fixed by the devicetree | **`analogWriteFrequency()`** |
| Result | `void` — failures are silent | `bool` — checkable |

If all you need is PWM on D9 at the devicetree frequency, the core is fine.
This library is for everything else.

## 🔌 Compatibility

* **Arduino UNO Q** (`arduino:zephyr:unoq`)
* Arduino Zephyr core **0.54.1**, **0.90.0**, **1.0.0**
* Both `link_mode=dynamic` (default) and `link_mode=static`
* The core's own `analogWrite()` keeps working unchanged — see
  [Interaction with the core](#-interaction-with-the-cores-analogwrite) below.

## 📍 Which pins get which engine

The board decides, and the sketch does not have to care.

| Engine | Pins | Character |
|---|---|---|
| **Hardware PWM** | D2, D3, D5, D6, D7, D8, D9, D10, D11, D12, D13, D20, D21, and the LED3 red/green/blue channels | Exact frequency, 1 µs pulse resolution, no CPU cost |
| **Software PWM** | D0, D1, D4, A0, A1, A2, A3, A4, A5, and the LED4 channels | Zephyr `k_timer`, **100 µs** granularity |

```cpp
bool fast = analogWritePinFrequency(9) == 1000.0f;   // hardware, exact
```

> The UNO Q silkscreen marks six pins with `~` (D3, D5, D6, D9, D10, D11),
> matching the UNO R3. The board actually routes 13 header pins to timers.
> Ask the library rather than trusting the marking.

### Resolution, honestly

* **Hardware pins** get the full 8 bits everywhere in the supported range.
* **Software pins** step in 100 µs units. At the default 1 kHz a period is only
  10 ticks, so a software pin has **10 duty steps**, not 256. Lower the
  frequency for finer control: at 50 Hz a period is 200 ticks.

`analogWriteTickUs()` reports the granularity.

## 📚 API

### Writing

| Function | Description |
|---|---|
| `analogWritePin(pin, value)` | Duty 0–255. Returns `true` when the pin is driving PWM. |
| `analogWriteStop(pin)` | Release one pin. Hardware pins are left at 0 %. |
| `analogWriteStarted(pin)` | `true` while a pin is being driven. |
| `analogWriteEnd()` | Release everything and stop the software engine. |

### Frequency

| Function | Description |
|---|---|
| `analogWriteFrequency(pin, hz)` | Set the frequency of the timer behind `pin`. Returns `false` for a plain GPIO. |
| `analogWriteFrequency(hz)` | Set the software engine frequency. Returns what was applied. |
| `analogWriteFrequency()` | The hardware frequency, in Hz. |
| `analogWritePinFrequency(pin)` | The frequency actually in use on `pin`, whichever engine backs it. |

Frequency is clamped to **1–2000 Hz**. Changing it re-applies each pin's current
duty, so you never have to repeat the `analogWritePin()` calls.

> **Channels on one STM32 timer share a prescaler, so hardware frequency is
> global to the library.** `analogWriteFrequency(pin, hz)` takes a pin because
> it also answers "is this pin even on a timer?", not because each pin can hold
> an independent frequency. Two hardware pins cannot run at two frequencies.

### Diagnostics

| Function | Description |
|---|---|
| `analogWriteTickUs()` | Software engine granularity, `100` on the UNO Q. |
| `analogWriteSoftwareChannelCount()` | How many channels the software engine is serving. |

## ⚠️ Interaction with the core's `analogWrite()`

**The core's `analogWrite()` is not replaced, and cannot be.** The core defines
`void analogWrite(pin_size_t, int)` as a strong symbol; a second strong
definition would fail to link, and a macro would rewrite the core's own
definition and break the build.

So both exist side by side:

```cpp
analogWrite(9, 128);        // core: timer pins only, silent failures
analogWritePin(9, 128);     // this library: every pin, reports success
```

Use `analogWritePin()` when you care whether the pin actually works, which on a
board where only 13 of the header pins have timers is most of the time.

## 🚀 Servo

The servo class matches `Arduino_HardwareServo`, so sketches move over unchanged.

```cpp
#include <HardwareServo.h>

HardwareServo servo;

void setup() {
  servo.attach(4);       // any pin, timer or not
  servo.write(90);
}
```

| Method | Description |
|---|---|
| `attach(pin, min, max)` | Attach to a pin. Returns the pin, or `INVALID_SERVO`. Defaults: 500–2500 µs. |
| `write(deg)` | Move to 0–180 degrees; out-of-range values are clamped. |
| `writeMicroseconds(us)` | Set the pulse width directly. |
| `read()` / `readMicroseconds()` | Last commanded angle or pulse width, or `-1` when detached. |
| `detach()` / `attached()` | Release the pin / report attachment. |

All servos share one 50 Hz frame rate, which is what the hardware allows. A
servo cannot be mixed with PWM at a different frequency in the same sketch.

> At 50 Hz the software engine stretches the 500–2500 µs servo range across
> only about 26 of its 256 duty steps, so **a hardware pin gives a servo
> noticeably smoother motion than D4 or A0**. `analogWritePinFrequency()` tells
> you which one you have.

## 🧪 Examples

| Example | Description |
|---|---|
| `Fade` | Ramps D4 (no timer) up and down — the case the core cannot do. |
| `ServoSweep` | Two servos sweeping, one on a timer pin and one on a software pin. |

## ⚠️ Notes

* **Do not drive the reserved pins** — the internal SPI ready line, the VREF
  analog switch, and BOOT0. The library exposes them like any other pin.
* **The software engine runs in interrupt context.** Its load grows with the
  number of *distinct pulse widths*, not the number of channels.
* `analogWriteEnd()` is for handing pins back; a sketch does not normally need it.
* `Serial` on the UNO Q is the Arduino Router's monitor, so output is only
  visible while a monitor is attached.

## 📄 License

MPL-2.0.
