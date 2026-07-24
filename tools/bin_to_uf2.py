#!/usr/bin/env python3
"""
Convert a raw .bin firmware file to UF2 format for drag-and-drop flashing.

Usage:
  python3 bin_to_uf2.py <input.bin> <output.uf2> <base_address> <family_id>

Example:
  python3 bin_to_uf2.py app.bin app.uf2 0x08008000 0x00c5c5c5
"""

import struct
import sys

UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END    = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_PAYLOAD_SIZE = 256
UF2_BLOCK_SIZE   = 512

def convert(bin_path, uf2_path, base_addr, family_id):
    with open(bin_path, 'rb') as f:
        data = f.read()

    # Pad to 256-byte boundary
    if len(data) % UF2_PAYLOAD_SIZE != 0:
        data += b'\x00' * (UF2_PAYLOAD_SIZE - (len(data) % UF2_PAYLOAD_SIZE))

    total_blocks = len(data) // UF2_PAYLOAD_SIZE

    with open(uf2_path, 'wb') as f:
        for block_num in range(total_blocks):
            addr = base_addr + block_num * UF2_PAYLOAD_SIZE
            payload = data[block_num * UF2_PAYLOAD_SIZE : (block_num + 1) * UF2_PAYLOAD_SIZE]

            block = struct.pack('<IIIIIII',
                UF2_MAGIC_START0,
                UF2_MAGIC_START1,
                UF2_FLAG_FAMILY_ID_PRESENT,
                addr,
                UF2_PAYLOAD_SIZE,
                block_num,
                total_blocks,
            )
            block += struct.pack('<I', family_id)
            block += payload
            block += b'\x00' * (UF2_BLOCK_SIZE - len(block) - 4)
            block += struct.pack('<I', UF2_MAGIC_END)

            f.write(block)

    print(f"Converted {len(data)} bytes -> {total_blocks} UF2 blocks")
    print(f"Output: {uf2_path}")

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.uf2> <base_address> <family_id>")
        sys.exit(1)

    bin_path = sys.argv[1]
    uf2_path = sys.argv[2]
    base_addr = int(sys.argv[3], 0)
    family_id = int(sys.argv[4], 0)
    convert(bin_path, uf2_path, base_addr, family_id)
