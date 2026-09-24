# unoq-pwm-driver

`analogWrite()` for every pin of the **Arduino UNO Q**, running as an Arduino App Lab app.

The PWM generation lives entirely on the STM32U585 in `sketch/sketch.ino`, using the
`HardwareAnalogWrite` library:

* pins with an STM32 timer channel (D2, D3, D5–D13, D20, D21, the LED3 channels) use **hardware PWM**
* **every other GPIO**, including D0, D1, D4 and A0–A5, uses the **software PWM engine**

So this app can emit PWM on **any** pin of the board, at a frequency the sketch chooses.

## What the sketch does

1. Sets the shared frame rate to 50 Hz.
2. Sweeps two servos in opposite directions — one on D9 (hardware) and one on D4
   (software only), proving both engines behave identically through the same API.
3. Breathes PWM on the software pin, exercising the engine on its own.

## Library location

The `HardwareAnalogWrite` library is installed in the board's user library folder:

```
~/Arduino/libraries/HardwareAnalogWrite/
```

The library sources are also copied into `sketch/` when the app is deployed,
because an App Lab sketch keeps its `sketch.yaml` and that puts `arduino-cli`
into profile mode, where the user library directory is not searched.

## Running

```bash
cd ~/ArduinoApps/unoq-pwm-driver
arduino-app-cli app restart .
arduino-app-cli monitor        # watch the sketch output
```

## Source of truth

The library is developed and version controlled in the `unoq-pwm-driver` git
repository; this App Lab folder is a deployment target. `deploy.ps1` copies
`src/Hardware*.{h,cpp}` here, so edit `src/` and redeploy rather than editing
the copies in `sketch/`.
