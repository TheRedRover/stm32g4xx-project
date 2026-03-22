# STM32 G4xx Bootloader + Firmware Project

This repository contains a dual-image STM32G474 project:

- **`nucleo-g4xx-bootloader/`**: Bootloader target
- **`nucleo-g4xx-firmware/`**: Main application firmware target
- **`shared/`**: Common headers/sources used by both projects (memory map,
  firmware header)
- **`scripts/build_image.py`**: Packaging script that creates a ready-to-flash
  image with metadata header

Security notes:
- The bootloader performs integrity and authenticity checks before booting
  firmware: CRC32 verification, SHA-256 hashing, and ECDSA P-256 signature
  verification (micro-ecc/uECC). A built-in public key is embedded into the
  bootloader binary at build time and used to verify the firmware signature.

The image format is driven by:

- `shared/Inc/memory_map.h` (flash layout and slot addresses)
- `shared/Inc/fw_header.h` (128-byte firmware header definition)

## Flash layout summary

From `shared/Inc/memory_map.h`:

- Bootloader starts at `0x08000000`, size `32 KB`
- Remaining flash is split into two slots
- Each slot has:
  - `1 KB` header (`FW_HDR_SIZE`)
  - firmware payload area (`FW_SIZE` and `FW_SIZE`)

## Prerequisites

Install tools required by both build and packaging:

- `cmake` (with preset support)
- `ninja`
- Arm GNU toolchain in PATH (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, ...)
- `python3`
- `srec_cat` (from the SRecord package)
- `git` (optional but recommended for embedded hash metadata)
 - `pip install ecdsa` (used by `scripts/build_image.py` to produce ECDSA P-256
   signatures)
 - `openssl` (recommended for generating ECDSA P-256 key pairs)

## Build binaries

From repository root:

### 1) Build bootloader (Release)

```bash
cd nucleo-g4xx-bootloader
cmake --preset Release
cmake --build --preset Release
```

Artifacts are generated in `nucleo-g4xx-bootloader/build/Release/` with names
like:

- `nucleo-g4xx-bootloader_<git>_<timestamp>.bin`
- `nucleo-g4xx-bootloader_<git>_<timestamp>.hex`

### 2) Build firmware (Release)

```bash
cd ../nucleo-g4xx-firmware
cmake --preset Release
cmake --build --preset Release
```

Artifacts are generated in `nucleo-g4xx-firmware/build/Release/` with names
like:

- `nucleo-g4xx-firmware_<git>_<timestamp>.bin`
- `nucleo-g4xx-firmware_<git>_<timestamp>.hex`
- `nucleo-g4xx-firmware_<git>_<timestamp>_OTA.bin`

> Note: firmware `CMakeLists.txt` already runs `scripts/build_image.py` in
> POST_BUILD to create `_OTA.bin` (header + firmware).

Important: the packaging step now signs the firmware. The `build_image.py`
script requires a private ECDSA P-256 key (PEM/DER) and will embed an ECDSA
signature (raw r|s, 64 bytes) into the 128-byte firmware header. The bootloader
verifies this signature at runtime.

## Generate a ready firmware image (manual script usage)

Use the script directly when you want full control over output and when
combining bootloader + firmware into a single image.

From repository root:

```bash
python3 scripts/build_image.py \
  --fw nucleo-g4xx-firmware/build/Release/nucleo-g4xx-firmware_*.bin \
  --bl nucleo-g4xx-bootloader/build/Release/nucleo-g4xx-bootloader_*.bin \
  --key secrets/keys/private_key.pem \
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
  - the header will include an ECDSA P-256 signature at offset 0 (raw r|s, 64
    bytes)

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
 - `--key` (required): ECDSA P-256 private key file (PEM or DER) used to sign
   the firmware. The build system also uses `secrets/keys/private_key.pem` to
   extract the public key and embed it into the bootloader binary.

## Output types

- `.bin`: flat binary with relative offsets (base depends on whether bootloader
  is included)
- `.hex`: Intel HEX with absolute MCU flash addresses

## Quick verification

After generation, check output size and expected metadata using your normal
flashing/inspection tooling.

## Security / Signing workflow

- Generate an ECDSA P-256 (prime256v1 / secp256r1) keypair if you don't have
  one:

```bash
openssl ecparam -name prime256v1 -genkey -noout -out secrets/keys/private_key.pem
openssl ec -in secrets/keys/private_key.pem -pubout -out secrets/keys/public_key.pem
```

- The `nucleo-g4xx-bootloader` CMake flow will extract the raw 64-byte
  uncompressed public key from `secrets/keys/private_key.pem` at build time and
  generate `generated/public_key.c` which defines `g_public_key[64]`. The
  bootloader uses `g_public_key` to verify firmware signatures.

- When creating release images manually, pass the same private key to
  `scripts/build_image.py` via `--key` so the header contains a signature the
  bootloader can verify.

## Notes

- The bootloader performs, in order: header magic/size checks, CRC32 of the
  firmware payload, SHA-256 of header+payload, and ECDSA P-256 signature
  verification. If all checks pass the bootloader jumps to the firmware.
- The bootloader supports twin bank verification and option-byte bank toggling
  via `Boot_ToggleBank()` / option bytes in the HAL.
