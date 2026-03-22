#!/usr/bin/env python3
import sys, os, struct, hashlib
from ecdsa import VerifyingKey, NIST256p
from ecdsa.util import sigdecode_string

# Usage: verify_image.py <image.bin> [public_key_c]
if len(sys.argv) < 2:
    print("Usage: verify_image.py <image.bin> [public_key_c]")
    sys.exit(2)

image_path = sys.argv[1]
pub_c = sys.argv[2] if len(sys.argv) > 2 else 'nucleo-g4xx-bootloader/build/Release/generated/public_key.c'

if not os.path.exists(image_path):
    print(f"Image not found: {image_path}")
    sys.exit(2)
if not os.path.exists(pub_c):
    print(f"Public key C file not found: {pub_c}")
    sys.exit(2)

# parse public key C
with open(pub_c, 'r', encoding='utf-8') as f:
    txt = f.read()
try:
    raw = txt.split('{',1)[1].split('}',1)[0]
    parts = [p.strip() for p in raw.split(',') if p.strip()]
    pub = bytes(int(x,16) for x in parts)
except Exception as e:
    print('Failed to parse public key:', e)
    sys.exit(2)

if len(pub) != 64:
    print('Public key length is not 64 bytes:', len(pub))
    sys.exit(2)

data = open(image_path,'rb').read()
# try header at offset 0
def try_offset(off):
    if len(data) < off + 128:
        return False, 'file too small for header at offset %d' % off
    hdr = data[off:off+128]
    signature = hdr[0:64]
    header_body = hdr[64:128]
    # parse fw_size and magic
    crc, = struct.unpack_from('<I', hdr, 64)
    magic, = struct.unpack_from('<I', hdr, 68)
    fw_size, = struct.unpack_from('<I', hdr, 72)
    version, = struct.unpack_from('<I', hdr, 76)
    # bounds check
    if len(data) < off + 128 + fw_size:
        return False, f'file too small for firmware (fw_size={fw_size}) at offset {off}'
    fw = data[off+128:off+128+fw_size]
    digest = hashlib.sha256(header_body + fw).digest()
    vk = VerifyingKey.from_string(pub, curve=NIST256p)
    try:
        ok = vk.verify_digest(signature, digest, sigdecode=sigdecode_string)
    except Exception as e:
        return False, f'verify exception: {e}\nDigest: {digest.hex()}\nSignature: {signature.hex()}\nMagic: 0x{magic:08X} fw_size:{fw_size} version:0x{version:08X}'
    return ok, f'Digest: {digest.hex()}\nSignature: {signature.hex()}\nMagic: 0x{magic:08X} fw_size:{fw_size} version:0x{version:08X}'

print('Verifying image:', image_path)
ok, info = try_offset(0)
if ok:
    print('[+] header at offset 0: signature VALID')
    print(info)
    sys.exit(0)
else:
    print('[-] header at offset 0: signature INVALID or check failed')
    print(info)

# search for magic 0x47344657 little-endian bytes
magic_le = b'\x57\x46\x34\x47'
pos = data.find(magic_le)
if pos >= 0:
    # magic is at header offset + 68, so header_off = pos - 68
    hdr_off = pos - 68
    print('Found magic at byte', pos, 'checking header at', hdr_off)
    if hdr_off >= 0:
        ok2, info2 = try_offset(hdr_off)
        if ok2:
            print('[+] header at offset %d: signature VALID' % hdr_off)
            print(info2)
            sys.exit(0)
        else:
            print('[-] header at offset %d: signature INVALID or check failed' % hdr_off)
            print(info2)
    else:
        print('Found magic near start; header offset negative, skipping')

# no success
sys.exit(1)
