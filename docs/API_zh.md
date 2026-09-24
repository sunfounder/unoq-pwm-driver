# HardwareAnalogWrite API 接口文档

> SunFounder 出品的 **Arduino UNO Q** 全引脚 `analogWrite` 驱动库
> 库版本 1.0.0 · 许可证 MPL-2.0 · 架构 `zephyr`

---

## 目录

1. [它解决什么问题](#1-它解决什么问题)
2. [快速开始](#2-快速开始)
3. [引脚与引擎](#3-引脚与引擎)
4. [API 参考](#4-api-参考)
5. [与 core 的 analogWrite 的关系](#5-与-core-的-analogwrite-的关系)
6. [HardwareServo 舵机类](#6-hardwareservo-舵机类)
7. [常量](#7-常量)
8. [分辨率与误差的实话](#8-分辨率与误差的实话)
9. [注意事项](#9-注意事项)

---

## 1. 它解决什么问题

Arduino Zephyr core 自带的 `analogWrite()` 有三个限制：

| | core 的 `analogWrite()` | 本库的 `analogWritePin()` |
|---|---|---|
| 无定时器的引脚（D0、D1、D4、A0–A5） | 退化成普通数字 开/关 | **真正的 PWM**（软件引擎） |
| 频率 | 被 devicetree 固定 | **`analogWriteFrequency()` 可改** |
| 返回值 | `void`，失败静默 | `bool`，可判断 |

UNO Q 上只有 13 个排针接了 STM32 定时器，其余引脚用 core 的 `analogWrite()` 只能得到开关效果。本库补齐了这部分。

## 2. 快速开始

```cpp
#include <HardwareAnalogWrite.h>

void setup() {
  analogWritePin(4, 128);          // D4 输出 50%，而 D4 没有定时器
  analogWriteFrequency(1000.0f);   // 软件引擎频率设为 1 kHz
}

void loop() {}
```

舵机：

```cpp
#include <HardwareServo.h>

HardwareServo servo;

void setup() {
  servo.attach(4);   // 任意引脚
  servo.write(90);
}

void loop() {}
```

## 3. 引脚与引擎

引擎由板子决定，sketch 不需要关心。

| 引擎 | 引脚 | 特性 |
|---|---|---|
| **硬件 PWM** | D2、D3、D5、D6、D7、D8、D9、D10、D11、D12、D13、D20、D21，以及 LED3 的 R/G/B | 频率精确、脉冲分辨率 1 µs、不占 CPU |
| **软件 PWM** | D0、D1、D4、A0、A1、A2、A3、A4、A5，以及 LED4 的 R/G/B | Zephyr `k_timer` 中断驱动，粒度 **100 µs** |

用 `analogWritePinFrequency(pin)` 可以查任意引脚当前的实际频率，不需要事先知道它属于哪个引擎。

> UNO Q 丝印上只有 6 个引脚标了 `~`（D3、D5、D6、D9、D10、D11），那是沿用 UNO R3 的标记。板子实际路由了 13 个排针到定时器。

## 4. API 参考

### 4.1 写入

#### `bool analogWritePin(pin_size_t pin, int value)`

在 `pin` 上输出 PWM 占空比。

| 参数 | 类型 | 说明 |
|---|---|---|
| `pin` | `pin_size_t` | Arduino 引脚号 |
| `value` | `int` | 占空比 **0–255**，超出范围会被夹紧 |
| **返回** | `bool` | `true` = 正在输出 PWM；`false` = 引脚越界或通道申请失败 |

- 首次调用某个引脚时会自动申请该通道（硬件引脚先做 pinctrl 路由，普通 GPIO 先 `pinMode(pin, OUTPUT)`）。
- 占空比换算：`high_us = period_us * value / 255`。
- 反相通道（定时器的 'N' 输出、共阳极 RGB LED）由驱动内部换算，**调用者始终按"高电平占周期的比例"理解占空比**。

### 4.2 频率

#### `bool analogWriteFrequency(pin_size_t pin, float hz)`

设置 `pin` 所在定时器的频率。

| 参数 | 类型 | 说明 |
|---|---|---|
| `pin` | `pin_size_t` | 只用它判断该引脚是否在定时器上 |
| `hz` | `float` | 频率 Hz，夹紧到 `[1.0, 2000.0]` |
| **返回** | `bool` | `false` 表示该引脚没有定时器（普通 GPIO 走软件频率） |

- **同一个 STM32 定时器的所有通道共用一个预分频器，所以硬件频率是本库全局的。** 传 `pin` 是为了回答"这个引脚到底在不在定时器上"，不代表每个引脚能独立设频率。
- 调用后会**按当前占空比在新周期上重新输出**所有已在运行的硬件通道，不需要重发 `analogWritePin()`。

#### `float analogWriteFrequency(float hz)`

设置**软件引擎**的频率（服务于所有非定时器引脚）。返回实际生效的值。调用后所有软件通道按原占空比在新周期上重新输出。

#### `float analogWriteFrequency()`

返回当前**硬件**频率。

#### `float analogWritePinFrequency(pin_size_t pin)`

返回该引脚实际使用的频率（硬件引脚返回硬件频率，普通 GPIO 返回软件频率）。引脚越界返回 `0.0f`。

### 4.3 释放与状态

| 函数 | 说明 |
|---|---|
| `void analogWriteStop(pin_size_t pin)` | 停止驱动并释放该引脚。硬件引脚留在 0% 占空比；软件引脚从引擎中摘除，不再贡献定时器中断 |
| `bool analogWriteStarted(pin_size_t pin)` | 该引脚是否正在被本库驱动 |
| `void analogWriteEnd()` | 释放全部引脚并停止软件引擎 |

### 4.4 诊断

| 函数 | 返回 | 说明 |
|---|---|---|
| `analogWriteTickUs()` | `uint32_t` | 软件引擎粒度，UNO Q 上是 **100**（= `1000000 / CONFIG_SYS_CLOCK_TICKS_PER_SEC`） |
| `analogWriteSoftwareChannelCount()` | `uint16_t` | 当前由软件引擎服务的通道数 |

## 5. 与 core 的 analogWrite 的关系

**core 的 `analogWrite()` 没有被替换，也无法替换。**

原因：core 在 `wiring_analog.cpp` 里把 `void analogWrite(pin_size_t, int)` 定义为**强符号**。再提供一份强定义会在链接期报 duplicate symbol；用宏替换则会把 core 自己的函数定义一起改写，直接编译失败。所以两者并存：

```cpp
analogWrite(9, 128);        // core 版本：只对定时器引脚有效，失败静默
analogWritePin(9, 128);     // 本库版本：任意引脚，返回成败
```

需要判断"这个引脚到底有没有真的输出"时用 `analogWritePin()`。

## 6. HardwareServo 舵机类

接口与 [`Arduino_HardwareServo`](https://github.com/arduino-libraries/Arduino_HardwareServo)（以及经典 `Servo` 库）一致，sketch 可直接迁移。由于 UNO Q 任意引脚都能出 PWM，**挂载舵机数量没有上限**。

#### `uint8_t attach(int pin, int min = 500, int max = 2500)`

| 参数 | 说明 |
|---|---|
| `pin` | Arduino 引脚号 |
| `min` | 对应 0° 的脉宽 µs，夹紧到 `[0, 19999]` |
| `max` | 对应 180° 的脉宽 µs，夹紧到 `[1, 20000]`；若 `<= min` 则取 `min + 1` |
| **返回** | 成功返回引脚号；失败返回 `INVALID_SERVO`（255） |

挂载时会先把帧率设到 50 Hz（硬件引擎和软件引擎都设），再输出 `min` 脉宽。

#### 其余方法

| 方法 | 说明 |
|---|---|
| `void write(int deg)` | 转到 0–180 度，超范围夹紧；未挂载时无操作 |
| `void writeMicroseconds(int us)` | 直接设脉宽 µs，夹紧到 `[min, max]` |
| `int read()` / `int readMicroseconds()` | 最近命令的角度 / 脉宽；未挂载返回 `-1` |
| `void detach()` | 停止并释放引脚 |
| `bool attached()` | 是否已挂载 |

> **所有舵机共享一个 50 Hz 帧率**，这是硬件决定的。同一个 sketch 里不能混用不同频率的舵机和 PWM。

## 7. 常量

| 宏 | 值 | 说明 |
|---|---|---|
| `ANALOG_WRITE_MAX` | `255` | 占空比最大值，对应 `analogWrite`/`analogRead` 的习惯 |
| `ANALOG_WRITE_DEFAULT_FREQ` | `1000.0f` | 默认频率 Hz |
| `ANALOG_WRITE_MIN_FREQ` | `1.0f` | 频率下限 |
| `ANALOG_WRITE_MAX_FREQ` | `2000.0f` | 频率上限 |
| `SERVO_PERIOD_US` | `20000` | 舵机帧长 20 ms |
| `SERVO_MIN_US` | `500` | 0° 默认脉宽 |
| `SERVO_MAX_US` | `2500` | 180° 默认脉宽 |
| `INVALID_SERVO` | `255` | `attach()` 失败返回值 |

`HardwareAnalogWritePins.h` 里还有两个由 devicetree 编译期推导的量，换板子会自动适配：

| 宏 | UNO Q 上的值 | 说明 |
|---|---|---|
| `UNOQ_PWM_PIN_COUNT` | 70 | 可寻址引脚总数 |
| `UNOQ_HW_PWM_COUNT` | 16 | 有硬件定时器通道的引脚数 |

## 8. 分辨率与误差的实话

- **硬件引脚**在 1–2000 Hz 全范围内都是完整的 **8 位**（256 级）。
- **软件引脚**以 100 µs 为一步。默认 1 kHz 时一个周期只有 **10 步**，也就是软件引脚的占空比实际只有 10 级，不是 256 级。想要更细就降频率：50 Hz 时一周期有 200 步。
- 软件引脚的占空比 1（`value = 1`）在 1 kHz 下会被向上取到 1 个 tick（100 µs），即实际占空比 10%，而不是 0.4%。**低占空比在软件引脚上会明显偏大**，需要精细低占空比时请用硬件引脚。
- 舵机场景：50 Hz 下 500–2500 µs 只落在 256 级中的约 26 级上，所以**舵机接硬件引脚比接 D4/A0 平滑得多**。

## 9. 注意事项

1. **不要驱动保留引脚**——内部 SPI ready 线、VREF 模拟开关、BOOT0。本库把它们当普通引脚暴露，不做拦截。
2. **软件引擎运行在中断上下文**，负载随**不同脉宽的数量**增长，而不是随通道数增长。
3. **硬件频率是全局的**，见 4.2。
4. 改频率会立即影响已在运行的通道，包括正在转动的舵机。
5. `analogWriteEnd()` 只在需要交还引脚时使用，普通 sketch 不需要调用。
6. `Serial` 在 UNO Q 上是 Arduino Router 的监视器，只有挂着监视器时才看得到输出。

---

## 附：示例

| 示例 | 说明 |
|---|---|
| `Fade` | 在 D4（无定时器）上淡入淡出——core 做不到的场景 |
| `ServoSweep` | 两个舵机反向扫动，一个在定时器引脚、一个在软件引脚 |
