#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path

MAGIC = 0x43454D53
VERSION = 1
JOB_COUNT = 7
WORDS = 4 + JOB_COUNT * 3 + 2
FORMAT = struct.Struct('<' + 'I' * WORDS)
SIZES = [1, 4, 4, 4, 4, 2, 4]
SOURCE_ADDRS = [
    0x40545020, 0x40545028, 0x405450D0, 0x405450D8,
    0x405450D4, 0x40544E70, 0x40545494,
]
CACHE_ADDRS = [0x42E6302C + 4 * i for i in range(JOB_COUNT)]
FLAG_ADDRS = [0x42E63425 + i for i in range(JOB_COUNT)]

def main():
    ap = argparse.ArgumentParser(description='Decode post-SMEC_Init snapshot')
    ap.add_argument('snapshot')
    args = ap.parse_args()
    data = Path(args.snapshot).read_bytes()
    if len(data) != FORMAT.size:
        raise SystemExit(f'bad size {len(data)}, expected {FORMAT.size}')
    values = FORMAT.unpack(data)
    magic, version, captured, count = values[:4]
    if magic != MAGIC or version != VERSION or count != JOB_COUNT:
        raise SystemExit(
            f'bad header magic=0x{magic:08X} version={version} count={count}')
    source = values[4:11]
    cached = values[11:18]
    flags = values[18:25]
    any_initialized, chip_status = values[25:27]
    print(f'captured={captured} jobs={count} any_initialized={any_initialized} '
          f'chip_status_60={chip_status}')
    print('job size source_addr source     cache_addr cache      flag_addr flag match')
    for i in range(JOB_COUNT):
        print(f'{i+1:3d} {SIZES[i]:4d} 0x{SOURCE_ADDRS[i]:08X} '
              f'0x{source[i]:08X} 0x{CACHE_ADDRS[i]:08X} '
              f'0x{cached[i]:08X} 0x{FLAG_ADDRS[i]:08X} '
              f'{flags[i]:4d} {"YES" if source[i] == cached[i] else "NO"}')

if __name__ == '__main__':
    main()
