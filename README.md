# STM32 G4xx Bootloader + Firmware Project

This repository contains a dual-image STM32G474 project:

- **`nucleo-g4xx-bootloader/`**: Bootloader target
- **`nucleo-g4xx-firmware/`**: Main application firmware target
- **`shared/`**: Common headers/sources used by both projects (memory map, firmware header)
- **`scripts/build_image.py`**: Packaging script that creates a ready-to-flash image with metadata header

The image format is driven by:

- `shared/Inc/memory_map.h` (flash layout and slot addresses)
- `shared/Inc/fw_header.h` (128-byte firmware header definition)

## Flash layout summary

From `shared/Inc/memory_map.h`:

- Bootloader starts at `0x08000000`, size `32 KB`
- Remaining flash is split into two slots
- Each slot has:
  - `1 KB` header (`FW_HDR_SIZE`)
  - firmware payload area (`FW_1_SIZE` and `FW_2_SIZE`)

## Prerequisites

Install tools required by both build and packaging:

- `cmake` (with preset support)
- `ninja`
- Arm GNU toolchain in PATH (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, ...)
- `python3`
- `srec_cat` (from the SRecord package)
- `git` (optional but recommended for embedded hash metadata)

## Build binaries

From repository root:

### 1) Build bootloader (Release)

```bash
cd nucleo-g4xx-bootloader
cmake --preset Release
cmake --build --preset Release
```

Artifacts are generated in `nucleo-g4xx-bootloader/build/Release/` with names like:

- `nucleo-g4xx-bootloader_<git>_<timestamp>.bin`
- `nucleo-g4xx-bootloader_<git>_<timestamp>.hex`

### 2) Build firmware (Release)

```bash
cd ../nucleo-g4xx-firmware
cmake --preset Release
cmake --build --preset Release
```

Artifacts are generated in `nucleo-g4xx-firmware/build/Release/` with names like:

- `nucleo-g4xx-firmware_<git>_<timestamp>.bin`
- `nucleo-g4xx-firmware_<git>_<timestamp>.hex`
- `nucleo-g4xx-firmware_<git>_<timestamp>_OTA.bin`

> Note: firmware `CMakeLists.txt` already runs `scripts/build_image.py` in POST_BUILD to create `_OTA.bin` (header + firmware).

## Generate a ready firmware image (manual script usage)

Use the script directly when you want full control over output and when combining bootloader + firmware into a single image.

From repository root:

```bash
python3 scripts/build_image.py \
  --fw nucleo-g4xx-firmware/build/Release/nucleo-g4xx-firmware_*.bin \
  --bl nucleo-g4xx-bootloader/build/Release/nucleo-g4xx-bootloader_*.bin \
  --mem-map shared/Inc/memory_map.h \
  --struct-hdr shared/Inc/fw_header.h \
  --version 1.0.0 \
  --git-auto \
  --out firmware/first_release.bin
```

What this command does:

1. Reads memory layout macros from `memory_map.h`
2. Reads `FW_MAGIC_NUMBER` from `fw_header.h`
3. Builds a 128-byte firmware header with:
   - magic
   - firmware size
   - packed version (`Major.Minor.Patch`)
   - firmware CRC32
   - timestamp
   - git hash
   - header CRC32
4. Uses `srec_cat` to stitch:
   - bootloader (optional)
   - header
   - firmware
5. Produces output as `.bin` or `.hex` depending on `--out`

## Script options

`build_image.py` supports:

- `--fw` (required): firmware binary
- `--bl` (optional): bootloader binary
- `--mem-map` (required): path to memory map header
- `--struct-hdr` (required): path to firmware header definition
- `--out` (required): output file (`.bin` or `.hex`)
- `--version`: semantic version in `X.Y` or `X.Y.Z` (default `1.0.0`)
- `--git-hash`: explicit 8-char hash (default `00000000`)
- `--git-auto`: fetch hash from current git repository

## Output types

- `.bin`: flat binary with relative offsets (base depends on whether bootloader is included)
- `.hex`: Intel HEX with absolute MCU flash addresses

## Quick verification

After generation, check output size and expected metadata using your normal flashing/inspection tooling.
