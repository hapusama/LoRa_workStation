# STM32H743VIT6 + Ra-02 Class A 工程

本工程从 `LoraSTMacL1_2019.03.28_修改main函数_实现classA_通用版(Branch4)` 移植而来，目标器件为 STM32H743VIT6，射频模块为安信可 Ra-02（SX1278）。开发过程中未给目标板或 Ra-02 上电，也未执行下载或调试连接。

## Keil 工程

- 打开 `LoraSTMacH7.uvprojx`，选择 `Target 1`，按 `F7` 编译。
- Device：`STM32H743VITx`。
- Device Pack：`Keil.STM32H7xx_DFP.4.1.3`。
- 编译宏：`USE_HAL_DRIVER, STM32H743xx, REGION_CN470`。
- Flash Algorithm：`STM32H7x_2048.FLM`，覆盖 2 MB Flash。
- Debug Driver：ST-Link，接口为 SWD。本机识别到的是 ST-Link，不是 J-Link。
- 编译产物：`Objects/LoraSTMacH7_Ra02.axf` 和 `Objects/LoraSTMacH7_Ra02.hex`。

## Ra-02 接线

下表沿用原 Branch4 工程的逻辑引脚。必须先确认你的 H743 板已经把这些 MCU 管脚引出，而且没有被板载器件占用。

| Ra-02 引脚 | 信号 | STM32H743VIT6 |
|---|---|---|
| 1、2、9、16 | GND | GND（全部共地） |
| 3 | 3.3V | 独立、稳定的 3.3V 电源 |
| 4 | RESET | PB0 |
| 5 | DIO0 | PB1 |
| 6 | DIO1 | PB5 |
| 7 | DIO2 | PB6 |
| 8 | DIO3 | PB7 |
| 10 | DIO4 | PB8 |
| 11 | DIO5 | PB9 |
| 12 | SCK | PA5 / SPI1_SCK |
| 13 | MISO | PA6 / SPI1_MISO |
| 14 | MOSI | PA7 / SPI1_MOSI |
| 15 | NSS | PA4 / GPIO 片选 |

Ra-02 只能使用 3.3V，不能接 5V。按 V1.1 规格，模块最大发射功率为 +18 dBm、峰值电流约 105 mA，建议 3.3V 电源具备至少 200 mA 的供电能力。发射前必须接好 410–525 MHz 天线。

SPI1 使用 Mode 0，时钟约 6.25 MHz，低于 Ra-02 规定的 10 MHz 上限。驱动限制频率为 410–525 MHz，并在 470 MHz 频段使用 `PA_BOOST`。

## ST-Link 接线

| ST-Link | STM32H743VIT6 |
|---|---|
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| VTref / 3.3V Sense | 目标板 3.3V（仅作电平参考） |
| NRST（建议） | 目标板 NRST |

ST-Link 的 VTref 通常不是给目标板供电的输出。烧录时先按板卡要求给 H743 板上电，再连接 ST-Link；Ra-02、H743 和 ST-Link 必须共地。

## 继承的应用参数

`apps/main.c`、`apps/common`、`mac`、`peripherals`、`radio`、`system` 共 56 个文件已与原 Branch4 工程做 SHA-256 字节级比对，内容一致。当前主要参数如下：

| 参数 | 值 |
|---|---|
| 工作模式 | Class A / `REGION_CN470` |
| 上行周期 | 3000 ms |
| 发射功率 | 2 dBm |
| 带宽 | 125 kHz |
| 扩频因子 | SF10 |
| 编码率 | 4/7 |
| 前导码 | 32 symbols |
| 上行信道 | 486.3–487.7 MHz，间隔 200 kHz，共 8 个 |
| RX1 / RX2 | 510.0 MHz / 505.3 MHz |
| RX2 SF | SF10 |
| RX1 / RX2 延迟 | 1000 ms / 2000 ms |
| DevAddr / AppPort | `0x11223344` / 88 |
| 应用负载 | 20 bytes |

## 板卡假设与首次上电检查

- 系统使用内部 HSI 经 PLL 得到 400 MHz SYSCLK，AHB 为 200 MHz，APB 为 100 MHz。
- RTC 使用 PC14/PC15 上的 32.768 kHz LSE。若板上没有 LSE 晶振，当前程序会停在时钟初始化，需改成 LSI 或补晶振后才能运行。
- 当前 H7 低功耗适配使用可由 RTC 唤醒的 Sleep 模式，未直接照搬 STM32L1 的 STOP 寄存器配置。
- 首次上电先不接 Ra-02，只确认 3.3V、GND、NRST 和 SWD 可连接；随后断电接好 Ra-02 和天线，再做整机上电测试。
- 若 Keil 能下载但无线不收发，优先核对 PA4–PA7、PB0、PB1、PB5–PB9 的实际板卡连线和占用情况。

官方资料：

- https://docs.ai-thinker.com/Ra-02/
- https://aithinker-static.oss-cn-shenzhen.aliyuncs.com/docs/Specification/ra-02_product_specification_zh_v1.1.pdf
