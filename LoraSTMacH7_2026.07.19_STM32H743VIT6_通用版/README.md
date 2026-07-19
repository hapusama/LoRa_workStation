# LoraSTMacH7 STM32H743VIT6 port

This is an independent STM32H743VIT6 port of the legacy LoraSTMacL1 project.
It does not modify the original STM32L152 workspace project.

## Purpose

- Verify that the STM32H743 toolchain configuration builds correctly.
- Verify SWD connection and internal Flash programming before porting LoRa.
- Provide the correct Cortex-M7 startup, device headers, and memory map.

## Build and download

1. Open `MDK-ARM/LoraSTMacH7.uvprojx` in Keil uVision.
2. Build with `F7`.
3. In `Options for Target > Debug`, select `ST-Link Debugger` and use `SW`.
4. In `Options for Target > Utilities`, select `ST-Link Debugger`.
5. Confirm that the Flash algorithm is the 2 MB STM32H7 internal Flash
   algorithm at `0x08000000`.
6. Download with `F8`.

The program intentionally does not drive a board LED because the custom board
pinout is not yet known. In a debug session, `g_heartbeat` should continuously
increase when the processor is running.

## Porting boundary

The LoRaWAN protocol code can later be moved from the STM32L152 project, but
the clock, GPIO, SPI, UART, RTC, interrupt, low-power, and SX1276 board files
must be implemented for the actual STM32H743 board pinout.

Third-party CMSIS source licenses are retained under
`THIRD_PARTY_LICENSES/`.
