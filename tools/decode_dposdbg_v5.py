#!/usr/bin/env python3
import argparse
import csv
import struct
from pathlib import Path

MAGIC = 0x30435254
VERSION = 5
HEADER = struct.Struct('<8I')
RECORD = struct.Struct('<16I')

FLAG_NAMES = {
    0: '69', 1: '6A', 2: '6B', 3: 'START', 4: '64',
    5: '65_DONE', 6: '67', 7: '66', 8: 'COMPLETE',
    9: 'TRUNCATED', 10: 'OVERFLOW', 11: 'UNEXPECTED',
    12: 'INCOMPLETE_PAYLOAD',
}


def flags_text(flags):
    return '|'.join(name for bit, name in FLAG_NAMES.items()
                    if flags & (1 << bit)) or '-'


def load(path):
    data = Path(path).read_bytes()
    if len(data) < HEADER.size:
        raise SystemExit('trace is shorter than the 32-byte header')

    meta = HEADER.unpack_from(data)
    magic, version, capacity, written, wrapped, exports, dropped, record_size = meta
    if magic != MAGIC:
        raise SystemExit(f'bad magic 0x{magic:08X}, expected TRC0')
    if version != VERSION:
        raise SystemExit(f'unsupported trace version {version}, expected {VERSION}')
    if record_size != RECORD.size:
        raise SystemExit(f'record size {record_size}, decoder expects {RECORD.size}')

    required = HEADER.size + capacity * record_size
    if len(data) < required:
        raise SystemExit(f'short trace: {len(data)} bytes, expected at least {required}')

    first = max(0, written - capacity)
    rows = []
    for absolute in range(first, written):
        slot = absolute % capacity
        values = RECORD.unpack_from(data, HEADER.size + slot * record_size)
        (seq, tick_ms, run_count, state_pack, flags,
         value_69, value_6a, value_6b, poll65_count,
         result_66, status_pack, counts_pack,
         begin_call_site, last_call_site,
         selector_count, primitive_count) = values
        if seq != absolute:
            continue

        row = {
            'sequence': seq,
            'tick_ms': tick_ms,
            'run_count': run_count,
            'run_stop': state_pack & 0xFF,
            'flags': flags,
            'flags_text': flags_text(flags),
            'value_69': value_69,
            'value_6a': value_6a,
            'value_6b': value_6b,
            'poll65_count': poll65_count,
            'result_66': result_66,
            'status_64': status_pack & 0xFF,
            'final_65': (status_pack >> 8) & 0xFF,
            'result_67': (status_pack >> 16) & 0xFF,
            'bytes_69': counts_pack & 0xFF,
            'bytes_6a': (counts_pack >> 8) & 0xFF,
            'bytes_6b': (counts_pack >> 16) & 0xFF,
            'bytes_66': (counts_pack >> 24) & 0xFF,
            'begin_call_site': begin_call_site,
            'last_call_site': last_call_site,
            'selector_count': selector_count,
            'primitive_count': primitive_count,
        }
        rows.append(row)

    return meta, rows


def mode_text(value):
    if value == 0:
        return 'RUN'
    if value == 1:
        return 'STOP'
    return f'0x{value:02X}'


def main():
    ap = argparse.ArgumentParser(description='Decode DPOSDBG v4.1 trace format v5')
    ap.add_argument('trace')
    ap.add_argument('--csv', dest='csv_path')
    args = ap.parse_args()

    meta, rows = load(args.trace)
    magic, version, capacity, written, wrapped, exports, dropped, record_size = meta
    print(f'version={version} capacity={capacity} written={written} '
          f'wrapped={wrapped} exports={exports} dropped={dropped} '
          f'record_size={record_size}')
    print('seq time_ms   d_ms mode run_count flags                              '
          'n69 n6a n6b n66 value6b    st64 f65 polls r67 result66 '
          'begin      last       sel primitives')

    previous_tick = None
    for r in rows:
        tick = r['tick_ms']
        delta = 0 if previous_tick is None else ((tick - previous_tick) & 0xFFFFFFFF)
        previous_tick = tick
        print(f"{r['sequence']:3d} {tick:7d} {delta:6d} "
              f"{mode_text(r['run_stop']):<4} {r['run_count']:9d} "
              f"{r['flags_text']:<34} "
              f"{r['bytes_69']:3d} {r['bytes_6a']:3d} "
              f"{r['bytes_6b']:3d} {r['bytes_66']:3d} "
              f"0x{r['value_6b']:08X} "
              f"0x{r['status_64']:02X} 0x{r['final_65']:02X} "
              f"{r['poll65_count']:5d} 0x{r['result_67']:02X} "
              f"0x{r['result_66']:08X} "
              f"0x{r['begin_call_site']:08X} "
              f"0x{r['last_call_site']:08X} "
              f"{r['selector_count']:4d} {r['primitive_count']:10d}")

    if args.csv_path:
        fields = list(rows[0].keys()) if rows else [
            'sequence', 'tick_ms', 'run_count', 'run_stop']
        with open(args.csv_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)


if __name__ == '__main__':
    main()
