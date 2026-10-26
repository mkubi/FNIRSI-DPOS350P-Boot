#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path

MAGIC = 0x30435254
HEADER = struct.Struct('<8I')
RECORD = struct.Struct('<16I')

ap = argparse.ArgumentParser(description='Decode SMEC_Init-only DPOSDBG startup jobs')
ap.add_argument('trace')
args = ap.parse_args()
data = Path(args.trace).read_bytes()
if len(data) < HEADER.size:
    raise SystemExit('trace shorter than header')
magic, version, capacity, written, wrapped, exports, dropped, record_size = HEADER.unpack_from(data)
if magic != MAGIC or version != 5 or record_size != RECORD.size:
    raise SystemExit(f'bad trace header magic=0x{magic:08X} version={version} record={record_size}')
if len(data) < HEADER.size + capacity * record_size:
    raise SystemExit('short trace body')
print(f'version={version} written={written} wrapped={wrapped} exports={exports} dropped={dropped}')
for absolute in range(max(0, written-capacity), written):
    off = HEADER.size + (absolute % capacity) * record_size
    v = RECORD.unpack_from(data, off)
    (seq, tick, runs, state, flags, value69, value6a, value6b, polls,
     result66, status, counts, begin, last, selectors, primitives) = v
    if seq != absolute:
        continue
    n69=counts&0xff; n6a=(counts>>8)&0xff; n6b=(counts>>16)&0xff; n66=(counts>>24)&0xff
    st64=status&0xff; f65=(status>>8)&0xff; st67=(status>>16)&0xff
    print(f'JOB {value6a & 0xff}: flags=0x{flags:08X} timing={value69 & 0xffff} payload=0x{value6b:08X}')
    print(f'  byte-counts 69/6A/6B/66 = {n69}/{n6a}/{n6b}/{n66}')
    print(f'  64=0x{st64:02X} 65=0x{f65:02X} polls65={polls} 67=0x{st67:02X} result66=0x{result66:08X}')
    print(f'  call-sites begin=0x{begin:08X} last=0x{last:08X} selectors={selectors} primitives={primitives}')
