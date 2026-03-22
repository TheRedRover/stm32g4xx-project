# STM32G4xx Bootloader + Firmware

Dual-slot bootloader and firmware for **STM32G474RETx** (Cortex-M4F @ 170MHz), written in Rust using [embassy-stm32](https://embassy.dev/).

The bootloader validates firmware via CRC32, supports OTA updates (Slot 2 -> Slot 1 copy), and jumps to the application.

## Flash Memory Layout

```
0x08000000  +--------------------------+
            |  Bootloader (32 KB)      |
0x08008000  +--------------------------+
            |  Slot 1 Header (1 KB)    |
0x08008400  +--------------------------+
            |  Slot 1 Firmware (239 KB)|
0x08044000  +--------------------------+
            |  Slot 2 Header (1 KB)    |
0x08044400  +--------------------------+
            |  Slot 2 Firmware (239 KB)|
0x08080000  +--------------------------+
```

## Prerequisites

```bash
# Rust nightly toolchain (installed automatically via rust-toolchain.toml)
rustup target add thumbv7em-none-eabihf

# ARM toolchain (for objcopy)
# Arch: pacman -S arm-none-eabi-binutils
# Debian/Ubuntu: apt install binutils-arm-none-eabi

# srecord (for OTA image stitching)
# Arch: pacman -S srecord
# Debian/Ubuntu: apt install srecord

# probe-rs (for flashing and defmt log output)
cargo install probe-rs-tools
```

## Build

```bash
./build.sh
```

This builds both crates in release mode, extracts `.bin` files, checks size limits, and generates the combined OTA image via `scripts/build_image.py`.

Output files in `target/thumbv7em-none-eabihf/release/`:
- `bootloader.bin` -- raw bootloader binary
- `firmware.bin` -- raw firmware binary
- `full_image.bin` -- stitched bootloader + header + firmware (ready to flash)

### Build individual crates

```bash
cargo build --release -p bootloader
cargo build --release -p firmware
```

### Run tests

```bash
cargo test -p shared --target x86_64-unknown-linux-gnu
```

Tests cover memory map constants, header struct layout, CRC32 correctness, and version packing.

### Flash

```bash
# Flash the combined image
probe-rs download --chip STM32G474RETx target/thumbv7em-none-eabihf/release/full_image.bin

# Or run firmware directly with defmt log output
cargo run --release -p firmware
```

### Set firmware version

```bash
VERSION=2.1.0 ./build.sh
```

## Project Structure

```
.
├── Cargo.toml                # Workspace root
├── rust-toolchain.toml       # Nightly toolchain config
├── build.sh                  # Full build + OTA image generation
├── bootloader/               # Bootloader (bin crate, 32 KB budget)
│   └── src/
│       ├── main.rs           # Boot flow: validate -> copy -> jump
│       ├── crc_hw.rs         # Hardware CRC32 via embassy CRC peripheral
│       ├── flash.rs          # Flash erase/write via embassy blocking API
│       ├── jump.rs           # Jump-to-app with inline assembly
│       └── error.rs          # LED error blink patterns
├── firmware/                 # Firmware (bin crate)
│   └── src/
│       └── main.rs           # Clock, LED blink, UART, header display
├── shared/                   # Shared library crate + C headers
│   ├── Inc/                  # C headers (used by build_image.py)
│   │   ├── memory_map.h      # Flash partition layout
│   │   └── fw_header.h       # Firmware header struct definition
│   └── src/                  # Rust source
│       ├── memory_map.rs     # Flash partition constants
│       ├── fw_header.rs      # 128-byte header struct + version helpers
│       └── crc.rs            # Software CRC32 (for host-side tests)
└── scripts/
    └── build_image.py        # OTA image generator (header + srec_cat)
```

## Hardware

- **Board**: NUCLEO-G474RE
- **MCU**: STM32G474RETx (512 KB flash, 128 KB RAM)
- **Clock**: HSI 16 MHz -> PLL -> 170 MHz
- **UART**: USART1 on PA9/PA10 @ 115200 baud
- **LED**: PA5 (green)
