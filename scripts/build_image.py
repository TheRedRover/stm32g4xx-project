#!/usr/bin/env python3

import os
import sys
import ast
import struct
import argparse
import subprocess
import re
import time
import zlib


def get_git_hash():
    """Fetch short git hash (8 chars) from the current repository."""
    try:
        result = subprocess.check_output(
            ["git", "rev-parse", "--short=8", "HEAD"],
            stderr=subprocess.STDOUT
        )
        return result.decode("utf-8").strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("[!] Warning: Could not fetch git hash. Using '00000000'.")
        return "00000000"


def validate_and_pack_version(version_str):
    """
    Validates version string (X.Y or X.Y.Z) and packs it into uint32_t.
    Scheme: Major (8-bit), Minor (8-bit), Patch (16-bit) -> 0xMMmmPPPP
    """
    pattern = r'^(\d+)\.(\d+)(?:\.(\d+))?$'
    match = re.match(pattern, version_str)

    if not match:
        print(f"[-] Error: Invalid version format '{version_str}'. Use X.Y or X.Y.Z (e.g., 1.4.12)")
        sys.exit(1)

    major, minor, patch = match.groups()
    major = int(major)
    minor = int(minor)
    patch = int(patch) if patch else 0

    if not (0 <= major <= 255 and 0 <= minor <= 255 and 0 <= patch <= 65535):
        print("[-] Error: Version out of range! (Major: 0-255, Minor: 0-255, Patch: 0-65535)")
        sys.exit(1)

    return (major << 24) | (minor << 16) | patch


def _safe_eval(expr):
    """
    Safely evaluate a simple arithmetic expression.
    Allowed: integers, +, -, *, //, /, %, <<, >>, &, |, ^, ~, parentheses.
    """
    allowed_binops = {
        ast.Add: lambda a, b: a + b,
        ast.Sub: lambda a, b: a - b,
        ast.Mult: lambda a, b: a * b,
        ast.FloorDiv: lambda a, b: a // b,
        ast.Div: lambda a, b: a // b,
        ast.Mod: lambda a, b: a % b,
        ast.LShift: lambda a, b: a << b,
        ast.RShift: lambda a, b: a >> b,
        ast.BitOr: lambda a, b: a | b,
        ast.BitAnd: lambda a, b: a & b,
        ast.BitXor: lambda a, b: a ^ b,
    }

    allowed_unary = {
        ast.UAdd: lambda a: +a,
        ast.USub: lambda a: -a,
        ast.Invert: lambda a: ~a,
    }

    def _eval(node):
        if isinstance(node, ast.Expression):
            return _eval(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
            return int(node.value)
        if isinstance(node, ast.UnaryOp) and type(node.op) in allowed_unary:
            return allowed_unary[type(node.op)](_eval(node.operand))
        if isinstance(node, ast.BinOp) and type(node.op) in allowed_binops:
            return allowed_binops[type(node.op)](_eval(node.left), _eval(node.right))
        raise ValueError(f"Unsupported expression: {expr!r}")

    tree = ast.parse(expr, mode="eval")
    return int(_eval(tree))


def parse_memory_map(filepath):
    """
    Parses memory_map.h and resolves integer expressions.
    Supports simple macros and references between them.
    """
    raw_macros = {}

    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            m = re.match(r'^\s*#define\s+([A-Za-z0-9_]+)\s+(.+)$', line)
            if not m:
                continue

            name, value = m.groups()

            # Strip comments
            value = value.split("/*")[0].split("//")[0].strip()

            # Expand helper macro forms like _U(0x1234) -> 0x1234
            value = re.sub(r'_U\((.*?)\)', r'\1', value)

            # Remove trailing 'U' suffix from numeric literals only
            value = re.sub(r'\b(0x[0-9A-Fa-f]+|\d+)U\b', r'\1', value)

            raw_macros[name] = value

    resolved = {}

    def resolve(name, stack=None):
        if name in resolved:
            return resolved[name]
        if name not in raw_macros:
            raise KeyError(f"Macro '{name}' not found")

        if stack is None:
            stack = set()
        if name in stack:
            raise ValueError(f"Circular macro reference detected at '{name}'")

        stack.add(name)
        expr = raw_macros[name]

        # Replace known macro names with their resolved numeric values
        for k in list(raw_macros.keys()):
            if k == name:
                continue
            if re.search(rf'\b{k}\b', expr):
                expr = re.sub(rf'\b{k}\b', str(resolve(k, stack)), expr)

        # Remove helper macros/typedef remnants if any
        expr = expr.strip()

        try:
            value = _safe_eval(expr)
            resolved[name] = value
            stack.remove(name)
            return value
        except Exception:
            stack.remove(name)
            raise ValueError(f"Could not evaluate macro '{name}' = {expr!r}")

    # Resolve everything that can be resolved
    for k in list(raw_macros.keys()):
        try:
            resolve(k)
        except Exception:
            pass

    return resolved


def parse_magic_number(filepath):
    """
    Extract FW_MAGIC_NUMBER. 
    Reads line by line to avoid catching structure fields or comments.
    """
    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            match = re.match(r'^#define\s+FW_MAGIC_NUMBER\s+(0x[0-9A-Fa-f]+|[0-9]+)', line)
            if match:
                val_str = match.group(1)
                magic = int(val_str, 0)
                return magic & 0xFFFFFFFF
                
    print("[!] ERROR: Could not find #define FW_MAGIC_NUMBER in", filepath)
    return 0


def generate_header(fw_bin_path, header_out_path, packed_version, git_hash, magic_num):
    """Generates the 128-byte binary header."""
    with open(fw_bin_path, "rb") as f:
        fw_data = f.read()

    fw_size = len(fw_data)
    fw_crc = zlib.crc32(fw_data) & 0xFFFFFFFF
    timestamp = int(time.time())

    git_hash_bytes = git_hash.encode("utf-8")[:8].ljust(8, b"\0")
    reserved = b"\0" * 96

    # 5x uint32 + char[8] + char[96] + uint32 = 128 bytes
    header_format = "<IIIII8s96s"
    header_data_no_crc = struct.pack(
        header_format,
        magic_num,
        fw_size,
        packed_version,
        fw_crc,
        timestamp,
        git_hash_bytes,
        reserved
    )

    header_crc = zlib.crc32(header_data_no_crc) & 0xFFFFFFFF
    header_data = header_data_no_crc + struct.pack("<I", header_crc)

    if len(header_data) != 128:
        raise RuntimeError(f"Header size is {len(header_data)}, expected 128")

    with open(header_out_path, "wb") as f:
        f.write(header_data)

    print(f"[+] Header Info | Magic: 0x{magic_num:08X} | Packed Ver: 0x{packed_version:08X} | Git: {git_hash}")
    return len(header_data)


def build_srec_command(mem_map, bootloader, header, firmware, output):
    """
    Build srec_cat command.

    BIN  -> offsets from 0
    HEX  -> absolute flash addresses
    """
    if not output.lower().endswith((".bin", ".hex")):
        raise ValueError("Output file must end with .bin or .hex")

    is_hex = output.lower().endswith(".hex")
    cmd = ["srec_cat"]

    bl_abs = mem_map["BL_START_ADDR"]
    hdr_abs = mem_map["FW_1_HDR_ADDR"]
    fw_abs = mem_map["FW_1_ADDR"]

    if is_hex:
        # Absolute addresses in flash
        if bootloader:
            cmd.extend([bootloader, "-binary", "-offset", hex(bl_abs)])
        cmd.extend([header, "-binary", "-offset", hex(hdr_abs)])
        cmd.extend([firmware, "-binary", "-offset", hex(fw_abs)])
        cmd.extend(["-o", output, "-Intel"])
    else:
        # Relative offsets starting from 0
        if bootloader:
            image_base = bl_abs
            cmd.extend([bootloader, "-binary", "-offset", "0x0"])
            cmd.extend([header, "-binary", "-offset", hex(hdr_abs - image_base)])
            cmd.extend([firmware, "-binary", "-offset", hex(fw_abs - image_base)])
        else:
            image_base = hdr_abs
            cmd.extend([header, "-binary", "-offset", "0x0"])
            cmd.extend([firmware, "-binary", "-offset", hex(fw_abs - image_base)])
        cmd.extend(["-o", output, "-binary"])

    return cmd


def run_srecord(mem_map, bootloader, header, firmware, output):
    """Stitch images using srec_cat."""
    cmd = build_srec_command(mem_map, bootloader, header, firmware, output)

    print("[+] Running:")
    print("    " + " ".join(cmd))

    subprocess.run(cmd, check=True)
    print(f"[+] Output generated: {output}")


def check_input_sizes(mem_map, fw_path, bl_path=None):
    """Basic partition size checks before generating output."""
    fw_size = os.path.getsize(fw_path)
    if fw_size > mem_map["FW_1_SIZE"]:
        print(f"[-] Error: firmware too large ({fw_size} > {mem_map['FW_1_SIZE']})")
        sys.exit(1)

    if bl_path:
        bl_size = os.path.getsize(bl_path)
        if bl_size > mem_map["BL_SIZE"]:
            print(f"[-] Error: bootloader too large ({bl_size} > {mem_map['BL_SIZE']})")
            sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Generate firmware header and stitch bootloader/header/firmware into BIN or HEX."
    )
    parser.add_argument("--fw", required=True, help="Firmware binary")
    parser.add_argument("--bl", help="Optional bootloader binary")
    parser.add_argument("--mem-map", required=True, help="Path to memory_map.h")
    parser.add_argument("--struct-hdr", required=True, help="Path to fw_header.h")
    parser.add_argument("--out", required=True, help="Output .bin or .hex")
    parser.add_argument("--version", default="1.0.0", help="Version string X.Y or X.Y.Z")
    parser.add_argument("--git-hash", default="00000000", help="Git hash to embed")
    parser.add_argument("--git-auto", action="store_true", help="Read git hash from current repo")

    args = parser.parse_args()

    final_git_hash = get_git_hash() if args.git_auto else args.git_hash
    packed_version = validate_and_pack_version(args.version)

    mem_map = parse_memory_map(args.mem_map)
    magic_num = parse_magic_number(args.struct_hdr)

    required_keys = ["BL_START_ADDR", "BL_SIZE", "FW_HDR_SIZE", "FW_1_HDR_ADDR", "FW_1_ADDR", "FW_1_SIZE"]
    for key in required_keys:
        if key not in mem_map:
            print(f"[-] Error: macro '{key}' not found or not resolved in {args.mem_map}")
            sys.exit(1)

    check_input_sizes(mem_map, args.fw, args.bl)

    temp_header = "temp_header.bin"

    try:
        header_size = generate_header(args.fw, temp_header, packed_version, final_git_hash, magic_num)
        if header_size > mem_map["FW_HDR_SIZE"]:
            print(f"[-] Error: generated header too large ({header_size} > {mem_map['FW_HDR_SIZE']})")
            sys.exit(1)

        run_srecord(mem_map, args.bl, temp_header, args.fw, args.out)
    finally:
        if os.path.exists(temp_header):
            os.remove(temp_header)


if __name__ == "__main__":
    main()