import os
import sys
import struct
import argparse
import subprocess
import re
import time
import zlib

def get_git_hash():
    """Fetches the short git hash (8 chars) from the current repository."""
    try:
        # Run git command to get the short SHA
        result = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], 
                                         stderr=subprocess.STDOUT)
        return result.decode('utf-8').strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("[!] Warning: Could not fetch git hash. Using '00000000' instead.")
        return "00000000"

def parse_memory_map(filepath):
    """Parses memory_map.h and calculates addresses and sizes."""
    macros = {}
    with open(filepath, 'r') as f:
        lines = f.readlines()
        
    for line in lines:
        match = re.match(r'^\s*#define\s+([A-Za-z0-9_]+)\s+(.+)$', line)
        if match:
            name, value = match.groups()
            value = re.sub(r'_U\((.*?)\)', r'\1', value)
            value = value.replace(' U', '').replace('U', '')
            value = value.split('/*')[0].split('//')[0].strip()
            macros[name] = value

    resolved = {}
    def resolve(name, expression):
        if name in resolved: return resolved[name]
        for k, v in macros.items():
            if k in expression:
                expression = re.sub(rf'\b{k}\b', str(resolve(k, v)), expression)
        try:
            val = eval(expression)
            resolved[name] = val
            return val
        except: return expression

    for k, v in macros.items():
        resolve(k, v)
    return resolved

def parse_magic_number(filepath):
    """Extracts FW_MAGIC_NUMBER from the header file."""
    with open(filepath, 'r') as f:
        content = f.read()
        match = re.search(r'#define\s+FW_MAGIC_NUMBER\s+(0x[0-9A-Fa-f]+)', content)
        if match:
            return int(match.group(1), 16) & 0xFFFFFFFF
    return 0x47344657

def calculate_crc32(data):
    """Standard CRC32 for STM32."""
    return zlib.crc32(data) & 0xFFFFFFFF

def generate_header(fw_bin_path, header_out_path, version, git_hash, magic_num):
    """Generates the 128-byte binary header."""
    with open(fw_bin_path, 'rb') as f:
        fw_data = f.read()
        
    fw_size = len(fw_data)
    fw_crc = calculate_crc32(fw_data)
    timestamp = int(time.time())
    
    # Git hash padding to 8 bytes
    git_hash_bytes = git_hash.encode('utf-8')[:8].ljust(8, b'\0')
    reserved = b'\0' * 96
    
    # Format: 5x uint32, char[8], char[96] = 124 bytes
    header_format = '<IIIII8s96s'
    header_data_no_crc = struct.pack(
        header_format, magic_num, fw_size, version, fw_crc, timestamp, git_hash_bytes, reserved
    )
    
    header_crc = calculate_crc32(header_data_no_crc)
    header_data = header_data_no_crc + struct.pack('<I', header_crc)
    
    with open(header_out_path, 'wb') as f:
        f.write(header_data)
    
    print(f"[+] Header Info | Magic: 0x{magic_num:08X} | Ver: {version} | Git: {git_hash} | CRC: 0x{fw_crc:08X}")
    return len(header_data)

def run_srecord(mem_map, bootloader, header, firmware, output):
    """Stitches images using srec_cat."""
    cmd = ['srec_cat']
    
    if bootloader:
        bl_addr = mem_map.get('BL_START_ADDR')
        cmd.extend([bootloader, '-binary', '-offset', hex(bl_addr)])

    hdr_addr = mem_map.get('FW_1_HDR_ADDR')
    cmd.extend([header, '-binary', '-offset', hex(hdr_addr)])

    fw_addr = mem_map.get('FW_1_ADDR')
    cmd.extend([firmware, '-binary', '-offset', hex(fw_addr)])

    cmd.extend(['-o', output])
    if output.lower().endswith('.hex'): cmd.append('-Intel')
    elif output.lower().endswith('.bin'): cmd.append('-binary')
    
    subprocess.run(cmd, check=True)
    print(f"[+] Output generated: {output}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--fw', required=True)
    parser.add_argument('--bl', help="Optional bootloader")
    parser.add_argument('--mem-map', required=True)
    parser.add_argument('--struct-hdr', required=True)
    parser.add_argument('--out', required=True)
    parser.add_argument('--version', type=int, default=1)
    parser.add_argument('--git-hash', default="00000000")
    parser.add_argument('--git-auto', action='store_true', help="Fetch hash from git automatically")
    
    args = parser.parse_args()

    # Determine git hash
    final_git_hash = get_git_hash() if args.git_auto else args.git_hash

    mem_map = parse_memory_map(args.mem_map)
    magic_num = parse_magic_number(args.struct_hdr)
    
    temp_header = "temp_header.bin"
    generate_header(args.fw, temp_header, args.version, final_git_hash, magic_num)
    run_srecord(mem_map, args.bl, temp_header, args.fw, args.out)

    if os.path.exists(temp_header): os.remove(temp_header)

if __name__ == "__main__":
    main()