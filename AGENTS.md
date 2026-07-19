# Repository Guidelines

## Project Structure & Module Organization

This workspace contains several LoRa-related projects. `gr-lora_sdr/` is the main GNU Radio 3.10 OOT module, with C++ blocks in `lib/`, public headers in `include/gnuradio/lora_sdr/`, Python bindings and hier blocks in `python/lora_sdr/`, GRC YAML in `grc/`, examples in `examples/`, simulations in `apps/simulation/`, and IQ/assets under `data/`. The two `LoraSTMacL1_*` folders are Keil STM32/SX1276 firmware projects. `debug_lora/` contains offline Python analysis scripts, and `dong/` contains USRP B210 GNU Radio flowgraphs and helper blocks.

## Build, Test, and Development Commands

- `cd gr-lora_sdr && .\build_grlora.bat`: configure, build, and install the GNU Radio module into the local Conda environment on Windows.
- `python gr-lora_sdr/examples/tx_rx_functionality_check.py`: run the basic software loopback check.
- `python gr-lora_sdr/apps/simulation/mc_simulator.py`: run Monte Carlo FER simulations.
- `python gr-lora_sdr/examples/lora_file_RX.py -f <iq.bin> --sf 10 --samp-rate 1000000 --bw 125000 --sync-word 0x34 --preamble-len 8 --has-crc`: decode an offline IQ capture.
- Open `LoraSTMacL1_*/LoraSTMac.uvprojx` in Keil uVision 5 and press `F7` to build firmware.

## Coding Style & Naming Conventions

Follow the existing GNU Radio OOT layout: one block header, implementation, pybind wrapper, and optional GRC YAML per block. C++ and Python files use lower-snake-case names such as `frame_sync_impl.cc` and `lora_file_RX.py`. Keep comments and Doxygen-style documentation in English. Prefer small, local changes that preserve existing APIs and directory boundaries.

## Testing Guidelines

Automated tests are limited; `gr-lora_sdr/lib/CMakeLists.txt` currently has empty C++ test sources. Validate changes with the closest functional script, offline IQ decode, or simulation run. For firmware changes, build in Keil and record target board, radio settings, and payload length used during validation.

## Commit & Pull Request Guidelines

Recent history uses short descriptive commits, sometimes in Chinese. Keep messages concise and action-oriented, for example `Fix SX1276 CRC mode` or `Add weak packet diagnostic plot`. Pull requests should describe the changed project, commands or hardware tests run, key parameters, and include plots/screenshots when analysis output changes.

## Security & Configuration Tips

Do not commit real LoRaWAN keys, device identifiers, or private IQ captures. Check SX1276 payload limits and sync-word, BW, SF, CR, CRC, and sample-rate settings before comparing hardware and SDR results.
