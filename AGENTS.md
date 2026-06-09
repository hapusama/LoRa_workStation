<!-- From: d:\Desktop\proj\AGENTS.md -->
# Agent Guidance for `proj`

This workspace contains **multiple independent projects** related to LoRa technology:

1. **`gr-lora_sdr/`** — A GNU Radio 3.10 out-of-tree (OOT) module implementing a complete LoRa transceiver in C++ and Python.
2. **`LoraSTMacL1_2019.03.28_修改main函数_实现classA_通用版(Branch4)/`** — An embedded LoRaWAN Class-A end-node firmware for STM32L152 + SX1276 (transmitter).
3. **`LoraSTMacL1_2022.06.01_用于数据包检测_接收端(va.2_无CAD_带连续 RSSI)/`** — A variant of the embedded firmware configured as a **continuous-reception packet detector / receiver** with RSSI logging.
4. **`debug_lora/`** — Python debugging and analysis scripts for offline IQ data inspection.
5. **`dong/`** — A GNU Radio Companion flowgraph and embedded Python blocks for USRP B210 operation.

> **Language note:** The source code comments and Doxygen documentation across all projects are written in **English**. Local READMEs and debugging notes in `gr-lora_sdr/README.md`, `debug_lora/README.md`, `dong/readme.md`, `grloraREADME.md`, and `重要提示.txt` are written in **Chinese**.

---

## 1. Project Overview

### 1.1 `gr-lora_sdr/`
A fully-functional **GNU Radio 3.10 out-of-tree (OOT) module** implementing a complete LoRa transceiver in C++ and Python. It supports end-to-end transmission/reception with USRPs or commercial LoRa radios (e.g., SX1276, SX1262, RFM95).

**Key capabilities:**
- Spreading factors 5–12
- Coding rates 0–4
- Implicit / explicit header modes
- CRC and explicit-header checksum verification
- Low data-rate optimisation (LDRO)
- Soft-decision decoding for improved receiver sensitivity
- Hierarchical Tx/Rx blocks for GNU Radio Companion (GRC)
- Monte-Carlo simulation framework for Frame-Error-Rate (FER) evaluation

**Local modifications (not in upstream):**
- **Dual-mode CRC verification** (`crc_verif` block): supports both `GRLORA` mode (default, for gr-lora_sdr ↔ gr-lora_sdr) and `SX1276` mode (for SX1276 hardware → software receiver).
- **Aligned preamble spectrogram export**: `frame_sync` block can publish aligned preamble + sync word + SFD sample ranges; Python scripts (`lora_file_RX.py`) save them as PNG spectrograms using `numpy + Pillow`.

**Origin:** Developed at the Telecommunication Circuits Laboratory, EPFL. Published in SPAWC 2020 and GNU Radio Proceedings 2024. Licensed under GPL-3.0-or-later.

### 1.2 `LoraSTMacL1_2019.03.28_修改main函数_实现classA_通用版(Branch4)/`
An **embedded LoRaWAN Class-A node firmware** targeting **STM32L152RCxxA** with a **Semtech SX1276** radio. It was derived from the Semtech LoRaMAC-node reference stack (copyright 2013–2017 Semtech) and modified in March 2019 to implement a generic Class-A version by rewriting `main.c`.

**Key characteristics:**
- Uses **Keil µVision 5** with ARMCC v5.06 as the build system.
- Configured for the **CN470** regional band (China 470 MHz).
- Implements a **lightweight custom Class-A state machine** directly in `main.c` rather than using the full `LoRaMac.c` state machine.
- Uses **ABP activation** with hard-coded test keys in `apps/Commissioning.h`.
- Transmits raw LoRaWAN frames every 5 s on a fixed channel and opens RX Window 1 and RX Window 2.
- **Fixed MIC placeholder**: `main.c` uses `uint32_t mic = 0x12345678;` as a hard-coded MIC value in both `SendFrame()` and `OnRxDone()`. This is **not** a computed AES-CMAC MIC.

### 1.3 `LoraSTMacL1_2022.06.01_用于数据包检测_接收端(va.2_无CAD_带连续 RSSI)/`
A **receiver-side variant** of the same embedded stack, modified in June 2022 for **continuous packet detection and RSSI recording**.

**Key differences from the Branch4 transmitter:**
- Operates in **continuous RX mode** (`Radio.Rx(0)`) rather than a Class-A tx/rx cycle.
- Logs **real-time RSSI values** into a buffer during packet reception (sampled every 5 ms).
- Prints packet number, payload content, RSSI, SNR, valid header count, and valid packet count for each received frame.
- No CAD (Channel Activity Detection) support.
- Used primarily to validate over-the-air transmissions from the Branch4 node or from `gr-lora_sdr`.

### 1.4 `debug_lora/`
A collection of **Python 3 offline analysis scripts** for debugging LoRa IQ captures and parameters:

| Script | Purpose |
|--------|---------|
| `analyze_sf.py` | Estimate actual spreading factor from IQ data using autocorrelation of instantaneous frequency |
| `analyze_sf_fast.py` | Faster variant of SF estimation |
| `analyze_sf_npy.py` | SF estimation for `.npy` format IQ files |
| `auto_decode_scan.py` | Automatic parameter scan for offline decoding |
| `crc_analysis.py` | Analyze CRC behavior across captured frames |
| `verify_sx1276_crc.py` | Verify SX1276-compatible CRC computation |
| `test_import.py` | Quick sanity check for Python imports |

These scripts reference IQ data stored in `gr-lora_sdr/data/USRP_IQ/`.

### 1.5 `dong/`
GNU Radio flowgraph files for **USRP B210** operation:
- `dong.grc` / `dong.py` — Flowgraph containing USRP source/sink parameters (including B210 serial numbers).
- `dong_epy_block_0.py` — Embedded Python block used inside the flowgraph.
- `rx_power_probe.py` — Custom probe for RX power measurement.

---

## 2. `gr-lora_sdr` — Detailed Guide

### 2.1 Technology Stack
| Layer | Technology |
|-------|------------|
| SDR Framework | GNU Radio 3.10 |
| Languages | C++ (signal-processing blocks), Python (bindings & flowgraphs) |
| Build System | CMake >= 3.16 |
| Python Bindings | pybind11 |
| Math/Signal Libs | VOLK 3.1.2, Boost 1.84.0, KISS FFT (bundled in `lib/kiss_fft.c`) |
| Hardware | UHD (USRP) |
| Packaging | Conda (conda-build / conda-smithy) |
| CI/CD | GitHub Actions (`.github/workflows/conda-build.yml`) |

### 2.2 Directory Layout
```
gr-lora_sdr/
├── CMakeLists.txt          # Root build configuration (version 1.0.0.git)
├── Changelog.md            # Upstream version history (current v0.5.8)
├── LICENSE                 # GPL-3.0
├── WINDOWS_SETUP.md        # Windows Conda install / reuse guide
├── environment.yml         # Linux Conda dev environment
├── environment-windows.yml # Windows Conda dev environment
├── build_grlora.bat        # One-click Windows build script (VS 2022 + conda)
├── apps/simulation/        # Monte-Carlo FER simulation framework
├── cmake/                  # Extra CMake modules
├── data/                   # Sample IQ captures and test vectors
│   ├── GRC_default/        # Text files for example transmit sources
│   └── USRP_IQ/            # Binary IQ captures from USRP
├── docs/                   # Doxygen setup
├── examples/               # Example flowgraphs (.grc + .py)
│   ├── lora_file_RX.py     # Offline IQ decoder (with CRC mode & preamble plot support)
│   ├── lora_file_preamble_fft.py
│   ├── lora_RX.py / lora_TX.py
│   ├── tx_rx_functionality_check.py
│   ├── tx_rx_simulation.py
│   ├── tx_rx_usrp.py
│   └── tx_rx_hier_functionality_check.py
├── grc/                    # GNU Radio Companion block YAMLs
├── include/gnuradio/lora_sdr/  # Public C++ headers
├── lib/                    # C++ implementations
└── python/lora_sdr/        # Python hier blocks + pybind11 bindings
```

### 2.3 Build Instructions

**From source (Windows with VS 2022 + conda — recommended for local development):**

A one-click script is provided:
```powershell
cd d:\Desktop\proj\gr-lora_sdr
.\build_grlora.bat
```

The script (`build_grlora.bat`) performs:
1. Calls `vcvarsall.bat x64` for Visual Studio 2022 Community edition
2. Removes and recreates the `build/` directory
3. Runs CMake with `NMake Makefiles` generator
4. Compiles with `nmake`
5. Installs to the conda environment (`D:\mysoft2\miniconda3\envs\gr-lora\Library`)

> **Note:** The script hardcodes the conda environment name `gr-lora` and the VS 2022 path. It also passes `-DENABLE_TESTING=OFF` and `-DENABLE_DOXYGEN=OFF`. If your environment name or VS installation path differs, edit `build_grlora.bat` before running.

**Manual build (Windows):**
```powershell
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d d:\Desktop\proj\gr-lora_sdr
rmdir /s /q build
mkdir build && cd build
set CMAKE_PREFIX_PATH=D:\mysoft2\miniconda3\envs\gr-lora\Library
cmake .. -G "NMake Makefiles" ^
    -DCMAKE_INSTALL_PREFIX=D:\mysoft2\miniconda3\envs\gr-lora\Library ^
    -DPYTHON_EXECUTABLE=D:\mysoft2\miniconda3\envs\gr-lora\python.exe ^
    -DGR_PYTHON_DIR=D:\mysoft2\miniconda3\envs\gr-lora\Lib\site-packages
nmake
nmake install
```

**From Conda (pre-built):**
```bash
conda create -n gr-lora python=3.10
conda activate gr-lora
conda install -c tapparelj -c conda-forge gnuradio-lora_sdr
```

**Key dependencies:** `gnuradio>=3.10`, `boost-cpp=1.84.0`, `cmake>=3.16`, `volk=3.1.2`, `pybind11`, `uhd`, `python>=3.10`.

### 2.4 Code Organization

Each GNU Radio block follows the standard OOT pattern: a public header in `include/` and a private implementation in `lib/`.

**Transmitter chain blocks:** `data_source`, `whitening`, `header`, `add_crc`, `hamming_enc`, `interleaver`, `gray_demap`, `modulate`, `RH_RF95_header`, `payload_id_inc`.

**Receiver chain blocks:** `frame_sync`, `fft_demod`, `gray_mapping`, `deinterleaver`, `hamming_dec`, `dewhitening`, `header_decoder`, `crc_verif`.

**Unused header:** `include/gnuradio/lora_sdr/no_sfo_frame_sync.h` exists but is **not** referenced in `lib/CMakeLists.txt` and has no corresponding implementation.

**Python layer:**
- `python/lora_sdr/lora_sdr_lora_tx.py` — hierarchical Tx block
- `python/lora_sdr/lora_sdr_lora_rx.py` — hierarchical Rx block
- `python/lora_sdr/bindings/` — one pybind11 wrapper per C++ block

### 2.5 Testing Strategy

- **Automated unit tests:** Minimal. `lib/CMakeLists.txt` contains `GrTest` scaffolding but the `test_lora_sdr_sources` list is currently **empty**. No C++ or Python unit-test suites are implemented. The `build_grlora.bat` script explicitly disables testing with `-DENABLE_TESTING=OFF`.
- **Installation verification:** The Conda recipe (`meta.yaml`) performs sanity checks: headers exist, shared library exists, GRC blocks load, `import gnuradio.lora_sdr` succeeds, and `.grc` files compile with `grcc`.
- **Functional validation:** Use the example scripts under `examples/` (e.g., `tx_rx_functionality_check.py`) or the simulation framework under `apps/simulation/`.
- **Offline validation:** Use `lora_file_RX.py` with IQ captures in `data/USRP_IQ/`.

### 2.6 Development Conventions

- **Licensing:** Every CMake file and most source files carry the SPDX identifier `GPL-3.0-or-later` and a copyright header.
- **Block naming:** Files and classes use lower-snake-case (e.g., `frame_sync`, `fft_demod`).
- **Adding a new block:** Follow the existing GNU Radio OOT layout (`include/` header, `lib/` implementation, `python/lora_sdr/bindings/` pybind11 wrapper, `grc/` YAML block definition).
- **Conda recipe updates:** If you change dependencies, update `.conda/recipe/meta.yaml` and re-render CI with `conda smithy rerender --feedstock_config .conda/conda-forge.yml -c auto`.

### 2.7 Known Issues (Local Build)

1. **pybind11 ABI mismatch:** If you see `ImportError: generic_type: type "..." referenced unknown base type "gr::block"`, the `pybind11` version used to compile `gr-lora_sdr` does not match the one used for `gnuradio-core`. Fix by aligning versions and recompiling:
   ```powershell
   conda install -c conda-forge pybind11=2.13.6
   rmdir /s /q build
   .\build_grlora.bat
   ```

2. **Python bindings out of sync:** If you modify a C++ header (e.g., `crc_verif.h`) and see a hash mismatch error, recompute the MD5 hash of the header and update the `BINDTOOL_HEADER_FILE_HASH(...)` macro in the corresponding `*_python.cc` binding file.

3. **GRC unavailable on Windows Conda:** The Windows Conda package does **not** include `gnuradio-companion`. Edit `.grc` files on a Linux/macOS machine, or directly modify the generated `.py` scripts.

4. **Linux absolute paths in examples:** Some example scripts (e.g., `tx_rx_simulation.py`) contain hard-coded Linux paths like `/home/jtappare/...`. Change them to local Windows paths before running.

---

## 3. `LoraSTMacL1` — Detailed Guide

### 3.1 Technology Stack
| Aspect | Details |
|--------|---------|
| Target MCU | STM32L152RCxxA (Cortex-M3, 256 KB Flash, 32 KB SRAM) |
| Radio IC | Semtech SX1276 |
| Language | C (C99) |
| Build System | Keil µVision 5 (`.uvprojx`) with ARMCC v5.06 update 7 |
| HAL/Library | STM32L1xx HAL Driver + CMSIS |
| Regional Band | CN470 |
| Debugger | UL2CM3 / ST-Link |

### 3.2 Directory Layout
```
LoraSTMacL1_.../
├── LoraSTMac.uvprojx       # Keil project file (primary build config)
├── startup_stm32l152xc.s   # ARM assembly startup
├── apps/                   # Application layer (main.c, interrupts, HAL conf, Commissioning.h)
├── boards/                 # Board support package (pin init, low power, sx1276-board.h)
├── fwlib/                  # STM32 firmware library (CMSIS + HAL)
├── mac/                    # LoRaMAC stack + region CN470
├── peripherals/soft-se/    # Software AES/CMAC secure element
├── radio/sx1276/           # SX1276 radio driver
├── system/                 # GPIO, timer, UART, FIFO, delay, systime
├── RTE/                    # Keil Runtime Environment
├── Objects/                # Build output (.axf, .hex)
└── Listings/               # Linker map files
```

### 3.3 Build Instructions
1. Open `LoraSTMac.uvprojx` in **Keil µVision 5**.
2. Select the target device `STM32L152RCxxA` (already configured).
3. Build (`F7`). Output files are generated in `Objects/`:
   - `LoraSTMac.axf` — ELF for debugging
   - `LoraSTMac.hex` — Intel HEX for flashing
4. Flash via ST-Link/UL2CM3.

### 3.4 Code Organization

- **`apps/main.c`** — The heart of the project. It does **not** call the full `LoRaMac.c` API. Instead, it manually constructs LoRaWAN MHDR + FHDR + FPort + Payload + MIC and drives the `Radio` abstraction directly. State machine: `INIT → SEND → CYCLE → SLEEP`.
- **`mac/`** — Full Semtech LoRaMAC-node stack is present (`LoRaMac.c`, crypto, regional parameters, etc.) but largely bypassed by the current `main.c`.
- **`radio/`** — Generic `radio.h` API and `sx1276/` register-level SPI driver.
- **`boards/`** — MCU clock init (32 MHz), UART1 (115200), GPIO mappings for LEDs, radio DIOs, and SPI pins.
- **`peripherals/soft-se/`** — Software AES-128 and CMAC implementation.

### 3.5 Known Issues & Important Notes

**Payload length limit bug (confirmed in BOTH LoraSTMacL1 variants):**
The SX1276 initialization table in `boards/sx1276-board.h` sets the LoRa max payload length register to `0x40` (64 bytes). For full LoRaWAN support this should be `0xFF` (255 bytes). The file `重要提示.txt` at the workspace root explicitly warns about this:

```c
// Current (limiting) value:
{ MODEM_LORA, REG_LR_PAYLOADMAXLENGTH, 0x40 },\

// Recommended fix:
{ MODEM_LORA, REG_LR_PAYLOADMAXLENGTH, 0xFF },\
```

If the receiver is configured with `0x40`, it can only receive packets ≤ 64 bytes. The transmitter will send any length successfully, but the receiver will be truncated. **Always verify this register when modifying the board support package.**

**Fixed MIC placeholder:**
`main.c` in the Branch4 variant uses a hard-coded `uint32_t mic = 0x12345678;` in `SendFrame()` and compares against the same value in `OnRxDone()`. This is **not** a real AES-CMAC MIC. It is acceptable for lab testing but must be replaced with proper MIC computation (using `LoRaMacCrypto.h` or `soft-se`) before any field deployment.

### 3.6 Security Considerations

- **Hard-coded keys:** `apps/Commissioning.h` contains static test keys (default Semtech test vectors) and a fixed DevAddr (`0x11223344`). This is acceptable for lab testing but **must be replaced with unique, per-device keys before any production or field deployment**.
- **No OTAA:** The project is configured for ABP only (`OVER_THE_AIR_ACTIVATION 0`). If OTAA is required, significant changes to `main.c` and key provisioning are needed.

---

## 4. Interoperability Notes (`gr-lora_sdr` ↔ `LoraSTMacL1`)

When decoding IQ captures from the `LoraSTMacL1` transmitter using `gr-lora_sdr`, ensure these parameters match exactly:

| Parameter | STM32/SX1276 Value | `gr-lora_sdr` / `lora_file_RX.py` Value |
|-----------|-------------------|----------------------------------------|
| **SF** | `LORA_SPREADING_FACTOR` (e.g., 10) | `--sf 10` (must match actual emitted SF) |
| **BW** | 125 kHz (`LORA_BANDWIDTH=0`) | `--bw 125000` |
| **CR** | 4/5 (`LORA_CODINGRATE=1`) | `--cr 1` |
| **CRC** | Hardware CRC on (`crcOn=true`) | `--has-crc` |
| **Header** | Explicit header (`fixLen=false`) | Default `impl_head=False` |
| **Sync Word** | `0x34` (`PublicNetwork=true`) | **Must use `--sync-word 0x34`** |
| **Preamble** | 8 symbols | **Must use `--preamble-len 8`** |
| **LDRO** | Auto (SF11/12 auto-enable) | `--ldro-mode 2` (Auto, default) |

**Critical: Sampling rate mismatch causes "SF+1" workaround**
The USRP IQ files in `gr-lora_sdr/data/USRP_IQ/` were captured at **1 MHz** sampling rate, while `lora_file_RX.py` defaults to 500 kHz. If you need `SF+1` to decode, it means the sampling rate is mismatched (higher sampling rate makes `Samples/Symbol` double, which is compensated by increasing SF by 1). **Always verify the actual capture sampling rate rather than using `SF+1` as a workaround.**

Correct command for 1 MHz captures:
```powershell
python gr-lora_sdr/examples/lora_file_RX.py `
    -f gr-lora_sdr/data/USRP_IQ/1_1_6_10_2_16.bin `
    --sf 10 --samp-rate 1000000 --bw 125000 --cr 1 `
    --sync-word 0x34 --preamble-len 8 --ldro-mode 2 --has-crc
```

---

## 5. Security Considerations (Workspace-wide)

1. **Cryptographic material:** Both projects involve LoRaWAN cryptography. Do not commit real device keys, AppEUI, or JoinEUI to version control.
2. **Conda environments:** The `gr-lora_sdr` repository contains `environment-lock.yml` and `conda_env_packages.txt` with exact package versions. When updating dependencies, review upstream changelogs for security fixes (especially `gnuradio`, `boost`, `uhd`).
3. **Embedded firmware:** The STM32 projects do not implement secure-boot or code-readout protection in the provided Keil configuration. Enable RDP (Read-Out Protection) and write-protect flash sectors when deploying firmware that contains secrets.
4. **Fixed MIC:** The Branch4 transmitter uses a placeholder MIC (`0x12345678`). Replace with proper AES-CMAC computation before any over-the-air deployment.

---

## 6. Quick Reference

| Task | Command / Action |
|------|------------------|
| Build `gr-lora_sdr` from source (Windows) | `cd d:\Desktop\proj\gr-lora_sdr && .\build_grlora.bat` |
| Install `gr-lora_sdr` via Conda | `conda install -c tapparelj -c conda-forge gnuradio-lora_sdr` |
| Run `gr-lora_sdr` loopback example | `python gr-lora_sdr/examples/tx_rx_functionality_check.py` |
| Run `gr-lora_sdr` Monte-Carlo sim | `python gr-lora_sdr/apps/simulation/mc_simulator.py` |
| Decode offline IQ (SX1276 source, 1 MHz) | `python gr-lora_sdr/examples/lora_file_RX.py -f <file> --sf 10 --samp-rate 1000000 --sync-word 0x34 --preamble-len 8 --crc-mode 0` |
| Build `LoraSTMacL1` firmware | Open `LoraSTMac.uvprojx` in Keil µVision 5 and press **F7** |
| Flash `LoraSTMacL1` firmware | Use Keil **Flash → Download** (ST-Link / UL2CM3) |
| Check payload limit bug | Inspect `boards/sx1276-board.h` for `REG_LR_PAYLOADMAXLENGTH` value |
| Fix payload limit | Change `0x40` → `0xFF` in `boards/sx1276-board.h` |

---

*This file was created based on the actual content of the workspace. If you restructure build systems, add tests, or change security-critical defaults, update this file accordingly.*
