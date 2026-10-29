#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path

MAGIC = 0x30435254
VERSION = 6
HEADER = struct.Struct('<8I')
RECORD = struct.Struct('<16I')
RAW_SELECTOR = 1 << 14
RAW_WRITE = 1 << 15
RAW_READ = 1 << 16


def main():
    ap = argparse.ArgumentParser(description='Decode DPOSDBG frontend T1 raw primitives')
    ap.add_argument('trace')
    ap.add_argument('--tail', type=int, default=0,
                    help='print only the last N raw primitive records')
    args = ap.parse_args()

    data = Path(args.trace).read_bytes()
    magic, version, capacity, written, wrapped, exports, dropped, record_size = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION or record_size != RECORD.size:
        raise SystemExit(
            f'bad trace magic=0x{magic:08X} version={version} record_size={record_size}')

    rows = []
    first = max(0, written - capacity)
    for absolute in range(first, written):
        values = RECORD.unpack_from(data, HEADER.size + (absolute % capacity) * record_size)
        seq, tick, run, state, flags, _, _, value, _, _, _, _, begin, _, _, _ = values
        if seq != absolute:
            continue
        if flags & RAW_SELECTOR:
            op = 'SEL'
        elif flags & RAW_WRITE:
            op = 'WR '
        elif flags & RAW_READ:
            op = 'RD '
        else:
            continue
        rows.append((seq, tick, run, state & 0xFF, begin, op, value & 0xFF))

    if args.tail > 0:
        rows = rows[-args.tail:]

    print(f'version={version} capacity={capacity} written={written} '
          f'wrapped={wrapped} exports={exports} raw_records={len(rows)}')
    print('sequence tick_ms run_count mode call_site  op value')
    for seq, tick, run, state, call, op, value in rows:
        mode = 'RUN' if state == 0 else ('STOP' if state == 1 else f'{state:02X}')
        print(f'{seq:8d} {tick:7d} {run:9d} {mode:4s} '
              f'0x{call:08X} {op} 0x{value:02X}')


if __name__ == '__main__':
    main()
