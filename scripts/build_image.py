#!/usr/bin/env python3
"""
Firmware image builder.

Pipeline:
  1. Parse memory_map.h  -> resolve all address/size macros
  2. Parse fw_header.h   -> extract FW_MAGIC_NUMBER
  3. Build 128-byte header (signature field zeroed)
  4. Sign SHA256(fw_binary || header_body) with ECDSA secp256r1 (P-256)
     Signature = raw r|s (32+32 = 64 bytes), compatible with micro-ecc uECC_verify()
  5. Write signature into header[0:64]
  6. Stitch [bootloader] + header + firmware via srec_cat -> .bin or .hex

Header layout (must match fw_header_t __packed__ in fw_header.h):
  Offset   Size   Field
  ------   ----   -----
       0     64   signature   (ECDSA r|s, zeroed while hashing)
      64      4   crc         (CRC32 of firmware binary)
      68      4   magic_number
      72      4   fw_size
      76      4   version     (packed: Maj8 | Min8 | Patch16)
      80      4   timestamp   (Unix time)
      84      8   git_hash
      92     36   reserved
  Total: 128 bytes

Dependencies:
  pip install ecdsa
  srec_cat must be on PATH
"""

import os
import sys
import ast
import struct
import argparse
import subprocess
import re
import time
import zlib
import hashlib

try:
    import ecdsa
    from ecdsa import SigningKey, NIST256p
    from ecdsa.util import sigencode_string
except ImportError:
    print("[-] Missing dependency: pip install ecdsa")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Git helpers
# ---------------------------------------------------------------------------

def get_git_hash():
    """Fetch short git hash (8 chars) from the current repository."""
    try:
        result = subprocess.check_output(
            ["git", "rev-parse", "--short=8", "HEAD"],
            stderr=subprocess.STDOUT,
        )
        return result.decode("utf-8").strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("[!] Warning: could not fetch git hash, using '00000000'.")
        return "00000000"


# ---------------------------------------------------------------------------
# Version helpers
# ---------------------------------------------------------------------------

def validate_and_pack_version(version_str):
    """
    Validate version string (X.Y or X.Y.Z) and pack into uint32_t.
    Layout: Major(8b) | Minor(8b) | Patch(16b) -> 0xMMmmPPPP
    """
    match = re.match(r'^(\d+)\.(\d+)(?:\.(\d+))?$', version_str)
    if not match:
        print(f"[-] Invalid version '{version_str}'. Use X.Y or X.Y.Z (e.g. 1.4.12)")
        sys.exit(1)

    major, minor, patch = match.groups()
    major, minor, patch = int(major), int(minor), int(patch) if patch else 0

    if not (0 <= major <= 255 and 0 <= minor <= 255 and 0 <= patch <= 65535):
        print("[-] Version out of range (Major:0-255, Minor:0-255, Patch:0-65535)")
        sys.exit(1)

    return (major << 24) | (minor << 16) | patch


# ---------------------------------------------------------------------------
# Safe arithmetic evaluator (for macro expansion)
# ---------------------------------------------------------------------------

def _safe_eval(expr):
    """Evaluate a simple C-style integer arithmetic expression."""
    allowed_binops = {
        ast.Add:      lambda a, b: a + b,
        ast.Sub:      lambda a, b: a - b,
        ast.Mult:     lambda a, b: a * b,
        ast.FloorDiv: lambda a, b: a // b,
        ast.Div:      lambda a, b: a // b,
        ast.Mod:      lambda a, b: a % b,
        ast.LShift:   lambda a, b: a << b,
        ast.RShift:   lambda a, b: a >> b,
        ast.BitOr:    lambda a, b: a | b,
        ast.BitAnd:   lambda a, b: a & b,
        ast.BitXor:   lambda a, b: a ^ b,
    }
    allowed_unary = {
        ast.UAdd:   lambda a: +a,
        ast.USub:   lambda a: -a,
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

    return int(_eval(ast.parse(expr, mode="eval")))


# ---------------------------------------------------------------------------
# memory_map.h parser
# ---------------------------------------------------------------------------

def parse_memory_map(filepath):
    """Parse memory_map.h and resolve all integer macros recursively."""
    raw_macros = {}

    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            m = re.match(r'^\s*#define\s+([A-Za-z0-9_]+)\s+(.+)$', line)
            if not m:
                continue
            name, value = m.groups()
            value = value.split("/*")[0].split("//")[0].strip()
            value = re.sub(r'_U\((.*?)\)', r'\1', value)
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
            raise ValueError(f"Circular reference at '{name}'")
        stack.add(name)
        expr = raw_macros[name]
        for k in list(raw_macros.keys()):
            if k != name and re.search(rf'\b{k}\b', expr):
                expr = re.sub(rf'\b{k}\b', str(resolve(k, stack)), expr)
        try:
            value = _safe_eval(expr.strip())
            resolved[name] = value
            stack.discard(name)
            return value
        except Exception:
            stack.discard(name)
            raise ValueError(f"Cannot evaluate '{name}' = {expr!r}")

    for k in list(raw_macros.keys()):
        try:
            resolve(k)
        except Exception:
            pass

    return resolved


def parse_magic_number(filepath):
    """Extract FW_MAGIC_NUMBER from fw_header.h."""
    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            m = re.match(
                r'^\s*#define\s+FW_MAGIC_NUMBER\s+(0x[0-9A-Fa-f]+|\d+)',
                line.strip()
            )
            if m:
                return int(m.group(1), 0) & 0xFFFFFFFF
    print(f"[!] FW_MAGIC_NUMBER not found in {filepath}")
    return 0


# ---------------------------------------------------------------------------
# ECDSA P-256 signing  (compatible with micro-ecc uECC_verify)
# ---------------------------------------------------------------------------

def load_signing_key(key_path):
    """Load a PEM or DER ECDSA P-256 private key."""
    with open(key_path, "rb") as f:
        data = f.read()
    try:
        sk = SigningKey.from_pem(data.decode("utf-8"))
    except Exception:
        sk = SigningKey.from_der(data)

    if sk.curve != NIST256p:
        print("[-] Key is not secp256r1 (NIST P-256). Aborting.")
        sys.exit(1)

    return sk


def sign_payload(fw_data, header_body, key_path):
    """
    Compute SHA-256(fw_data || header_body) and sign with ECDSA P-256.

    header_body: 64 bytes = header[64:128] (everything except signature field).

    Returns raw r|s bytes (64 bytes) — the format expected by
    micro-ecc uECC_verify(hash, sig, pubkey, curve).
    """
    sk = load_signing_key(key_path)

    digest = hashlib.sha256(fw_data + header_body).digest()
    signature = sk.sign_digest(digest, sigencode=sigencode_string)

    if len(signature) != 64:
        raise RuntimeError(f"Unexpected signature length: {len(signature)}")

    print(f"[+] SHA-256 : {digest.hex()}")
    print(f"[+] Sig r|s : {signature.hex()}")
    return signature


# ---------------------------------------------------------------------------
# Header builder
# ---------------------------------------------------------------------------

# Body layout: everything after signature[64]
# < I       I            I       I        I          8s        36s
#   crc     magic        fw_size version  timestamp  git_hash  reserved
_BODY_FMT  = "<IIIII8s36s"
_BODY_SIZE = struct.calcsize(_BODY_FMT)   # must be 64
_SIG_SIZE  = 64
_HDR_TOTAL = _SIG_SIZE + _BODY_SIZE       # must be 128

assert _BODY_SIZE == 64,  f"Body size mismatch: {_BODY_SIZE}"
assert _HDR_TOTAL == 128, f"Header total mismatch: {_HDR_TOTAL}"


def generate_header(fw_bin_path, header_out_path, packed_version, git_hash, magic_num, key_path):
    with open(fw_bin_path, "rb") as f:
        fw_data = f.read()

    fw_size       = len(fw_data)
    timestamp     = int(time.time())
    git_hash_bytes = git_hash.encode("utf-8")[:8].ljust(8, b"\x00")

    TAIL_FMT = "<IIII8s36s"
    header_tail = struct.pack(
        TAIL_FMT,
        magic_num,
        fw_size,
        packed_version,
        timestamp,
        git_hash_bytes,
        b"\x00" * 36,
    )

    fw_crc = zlib.crc32(header_tail + fw_data) & 0xFFFFFFFF

    header_body = struct.pack("<I", fw_crc) + header_tail

    signature = sign_payload(header_body, fw_data, key_path) 

    header_data = signature + header_body
    
    with open(header_out_path, "wb") as f:
        f.write(header_data)

    return len(header_data)


# ---------------------------------------------------------------------------
# srec_cat stitching
# ---------------------------------------------------------------------------

def build_srec_command(mem_map, bootloader, header, firmware, output):
    """
    Build srec_cat command.
    .hex  -> absolute flash addresses
    .bin  -> relative offsets from image base (0x0)
    """
    if not output.lower().endswith((".bin", ".hex")):
        raise ValueError("Output must end with .bin or .hex")

    is_hex  = output.lower().endswith(".hex")
    cmd     = ["srec_cat"]
    bl_abs  = mem_map["BL_1_START_ADDR"]
    hdr_abs = mem_map["FW_1_HDR_ADDR"]
    fw_abs  = mem_map["FW_1_ADDR"]

    if is_hex:
        if bootloader:
            cmd.extend([bootloader, "-binary", "-offset", hex(bl_abs)])
        cmd.extend([header,   "-binary", "-offset", hex(hdr_abs)])
        cmd.extend([firmware, "-binary", "-offset", hex(fw_abs)])
        cmd.extend(["-o", output, "-Intel"])
    else:
        if bootloader:
            image_base = bl_abs
            cmd.extend([bootloader, "-binary", "-offset", "0x0"])
            cmd.extend([header,   "-binary", "-offset", hex(hdr_abs - image_base)])
            cmd.extend([firmware, "-binary", "-offset", hex(fw_abs  - image_base)])
        else:
            image_base = hdr_abs
            cmd.extend([header,   "-binary", "-offset", "0x0"])
            cmd.extend([firmware, "-binary", "-offset", hex(fw_abs  - image_base)])
        cmd.extend(["-o", output, "-binary"])

    return cmd


def run_srecord(mem_map, bootloader, header, firmware, output):
    cmd = build_srec_command(mem_map, bootloader, header, firmware, output)
    print("[+] srec_cat: " + " ".join(cmd))
    subprocess.run(cmd, check=True)
    print(f"[+] Output  : {output}")


# ---------------------------------------------------------------------------
# Size validation
# ---------------------------------------------------------------------------

def check_input_sizes(mem_map, fw_path, bl_path=None):
    fw_size = os.path.getsize(fw_path)
    if fw_size > mem_map["FW_SIZE"]:
        print(f"[-] Firmware too large: {fw_size} > {mem_map['FW_SIZE']} bytes")
        sys.exit(1)
    if bl_path:
        bl_size = os.path.getsize(bl_path)
        if bl_size > mem_map["BL_SIZE"]:
            print(f"[-] Bootloader too large: {bl_size} > {mem_map['BL_SIZE']} bytes")
            sys.exit(1)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Sign and package firmware: header + [BL] + FW -> .bin/.hex"
    )
    parser.add_argument("--fw",         required=True,
                        help="Firmware binary (.bin)")
    parser.add_argument("--bl",
                        help="Bootloader binary (optional)")
    parser.add_argument("--key",        required=True,
                        help="ECDSA P-256 private key file (PEM or DER)")
    parser.add_argument("--mem-map",    required=True,
                        help="Path to memory_map.h")
    parser.add_argument("--struct-hdr", required=True,
                        help="Path to fw_header.h")
    parser.add_argument("--out",        required=True,
                        help="Output image (.bin or .hex)")
    parser.add_argument("--version",    default="1.0.0",
                        help="Version string X.Y or X.Y.Z  (default: 1.0.0)")
    parser.add_argument("--git-hash",   default="00000000",
                        help="8-char git hash to embed (default: 00000000)")
    parser.add_argument("--git-auto",   action="store_true",
                        help="Read git hash automatically from repo")
    args = parser.parse_args()

    final_git_hash = get_git_hash() if args.git_auto else args.git_hash
    packed_version = validate_and_pack_version(args.version)

    mem_map   = parse_memory_map(args.mem_map)
    magic_num = parse_magic_number(args.struct_hdr)

    required_keys = [
        "BL_1_START_ADDR", "BL_SIZE",
        "FW_HDR_SIZE", "FW_1_HDR_ADDR", "FW_1_ADDR", "FW_SIZE",
    ]
    for key in required_keys:
        if key not in mem_map:
            print(f"[-] Macro '{key}' not resolved in {args.mem_map}")
            sys.exit(1)

    check_input_sizes(mem_map, args.fw, args.bl)

    temp_header = "temp_fw_header.bin"
    try:
        header_size = generate_header(
            fw_bin_path=args.fw,
            header_out_path=temp_header,
            packed_version=packed_version,
            git_hash=final_git_hash,
            magic_num=magic_num,
            key_path=args.key,
        )
        if header_size > mem_map["FW_HDR_SIZE"]:
            print(f"[-] Header too large: {header_size} > {mem_map['FW_HDR_SIZE']}")
            sys.exit(1)

        run_srecord(mem_map, args.bl, temp_header, args.fw, args.out)
    finally:
        if os.path.exists(temp_header):
            os.remove(temp_header)


if __name__ == "__main__":
    main()