#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path

BLOCKS = 34
EDGE = 64
HEADER = struct.Struct('<16I')
EXPECTED = HEADER.size + EDGE * 4 + BLOCKS * 4 * 2

ap = argparse.ArgumentParser(description='Decode stock CSI destination buffer before/after snapshot')
ap.add_argument('snapshot')
args = ap.parse_args()
data = Path(args.snapshot).read_bytes()
if len(data) != EXPECTED:
    raise SystemExit(f'bad size {len(data)}, expected {EXPECTED}')

v = HEADER.unpack_from(data)
(magic, version, before, after, dst_reg, dst_addr, size, block_size,
 block_count, hash_before, hash_after, changed_count, first_changed,
 last_changed, status_before, status_after) = v
if magic != 0x46554243 or version != 1 or block_count != BLOCKS:
    raise SystemExit('bad CSIBUF header')

o = HEADER.size
prefix_before = data[o:o+EDGE]; o += EDGE
prefix_after = data[o:o+EDGE]; o += EDGE
suffix_before = data[o:o+EDGE]; o += EDGE
suffix_after = data[o:o+EDGE]; o += EDGE
block_before = struct.unpack_from('<' + 'I' * BLOCKS, data, o); o += BLOCKS * 4
block_after = struct.unpack_from('<' + 'I' * BLOCKS, data, o)
changed = [i for i, (a, b) in enumerate(zip(block_before, block_after)) if a != b]

def dump(label, raw):
    print(label)
    for i in range(0, len(raw), 16):
        print(f'  {i:04X}: ' + ' '.join(f'{x:02X}' for x in raw[i:i+16]))

print(f'version={version} captured_before={before} captured_after={after}')
print(f'destination_register=0x{dst_reg:08X} destination_address=0x{dst_addr:08X}')
print(f'buffer_size=0x{size:X} ({size}) block_size={block_size} blocks={block_count}')
print(f'hash_before=0x{hash_before:08X} hash_after=0x{hash_after:08X} changed={"YES" if hash_before != hash_after else "NO"}')
print(f'changed_blocks={changed_count} first={first_changed} last={last_changed} list={changed}')
print(f'csi_status_before=0x{status_before:08X} csi_status_after=0x{status_after:08X}')
dump('prefix_before:', prefix_before)
dump('prefix_after:', prefix_after)
dump('suffix_before:', suffix_before)
dump('suffix_after:', suffix_after)
