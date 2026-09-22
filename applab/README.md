# unoq-pwm-driver

Full-pin PWM / servo demo for the **Arduino UNO Q**, running as an Arduino App Lab app.

The PWM generation lives entirely on the STM32U585 in `sketch/sketch.ino`, using the
`UNOQ_PWMServoDriver` library:

* pins with an STM32 timer channel (D2, D3, D5–D13, D20, D21, RGB LED) use hardware PWM
* **every other GPIO**, including D0, D1, D4 and A0–A5, uses the software PWM engine

So this app can emit PWM on **any** pin of the board.

## What the sketch does

1. Prints the engine split (how many channels are hardware vs software).
2. Sweeps two servos in opposite directions — one on D9 (hardware) and one on D4
   (software only), proving both engines behave identically through the same API.
3. Breathes PWM on every header pin.

## Library location

The library is installed in the board's user library folder:

```
~/Arduino/libraries/UNOQ_PWMServoDriver/
```

## Running

```bash
cd ~/ArduinoApps/unoq-pwm-driver
arduino-app-cli app restart .
arduino-app-cli monitor        # watch the sketch output
```

## Source of truth

The library is developed and version controlled in the `unoq-pwm-driver` git
repository; this App Lab folder is a deployment target.
