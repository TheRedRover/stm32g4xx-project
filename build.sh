#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

VERSION="${VERSION:-1.0.0}"

echo "=== Building bootloader ==="
cargo build --release -p bootloader

echo "=== Building firmware ==="
cargo build --release -p firmware

echo "=== Extracting binaries ==="
TARGET_DIR="target/thumbv7em-none-eabihf/release"

arm-none-eabi-objcopy -O binary "$TARGET_DIR/bootloader" "$TARGET_DIR/bootloader.bin"
arm-none-eabi-objcopy -O binary "$TARGET_DIR/firmware" "$TARGET_DIR/firmware.bin"

echo "=== Binary sizes ==="
arm-none-eabi-size "$TARGET_DIR/bootloader" "$TARGET_DIR/firmware"

echo ""
echo "Bootloader .bin: $(stat -c%s "$TARGET_DIR/bootloader.bin") bytes (max 32768)"
echo "Firmware .bin:   $(stat -c%s "$TARGET_DIR/firmware.bin") bytes (max 244736)"

# Check size limits
BL_SIZE=$(stat -c%s "$TARGET_DIR/bootloader.bin")
FW_SIZE=$(stat -c%s "$TARGET_DIR/firmware.bin")

if [ "$BL_SIZE" -gt 32768 ]; then
    echo "ERROR: Bootloader exceeds 32KB limit!"
    exit 1
fi

if [ "$FW_SIZE" -gt 244736 ]; then
    echo "ERROR: Firmware exceeds 239KB limit!"
    exit 1
fi

echo ""
echo "=== Generating OTA image ==="
GIT_HASH=$(git rev-parse --short=8 HEAD 2>/dev/null || echo "00000000")

if command -v srec_cat &>/dev/null; then
    python3 scripts/build_image.py \
        --fw "$TARGET_DIR/firmware.bin" \
        --bl "$TARGET_DIR/bootloader.bin" \
        --mem-map shared/Inc/memory_map.h \
        --struct-hdr shared/Inc/fw_header.h \
        --out "$TARGET_DIR/full_image.bin" \
        --version "$VERSION" \
        --git-hash "$GIT_HASH"

    echo ""
    echo "=== Done ==="
    echo "Full image: $TARGET_DIR/full_image.bin"
    echo "Flash with: probe-rs download --chip STM32G474RETx --binary-format bin --base-address 0x08000000 $TARGET_DIR/full_image.bin"
else
    echo "WARNING: srec_cat not found, skipping OTA image generation."
    echo "Install srecord: sudo pacman -S srecord  (or apt install srecord)"
    echo ""
    echo "=== Done (binaries only) ==="
    echo "Bootloader: $TARGET_DIR/bootloader.bin"
    echo "Firmware:   $TARGET_DIR/firmware.bin"
fi
