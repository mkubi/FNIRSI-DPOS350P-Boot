#!/usr/bin/env python3
import argparse
import csv
import struct
from pathlib import Path

MAGIC = 0x30435254
HEADER = struct.Struct('<8I')
RECORD_V2 = struct.Struct('<9I')
RECORD_V3 = struct.Struct('<12I')
FLAG_NAMES = {
    0: '69', 1: '6A', 2: '6B', 3: 'START', 4: '64', 5: '65_DONE',
    6: '67', 7: '66', 8: 'COMPLETE', 9: 'TRUNCATED',
    10: 'OVERFLOW', 11: 'UNEXPECTED',
}

def flags_text(flags):
    return '|'.join(name for bit, name in FLAG_NAMES.items()
                    if flags & (1 << bit)) or '-'

def load(path):
    data = Path(path).read_bytes()
    if len(data) < HEADER.size:
        raise SystemExit('trace is shorter than the 32-byte header')
    header = HEADER.unpack_from(data)
    magic, version, capacity, write_index, wrapped, exports, dropped, record_size = header
    if magic != MAGIC:
        raise SystemExit(f'bad magic 0x{magic:08X}, expected TRC0')
    record = {2: RECORD_V2, 3: RECORD_V3}.get(version)
    if record is None:
        raise SystemExit(f'unsupported trace version {version}')
    if record_size != record.size:
        raise SystemExit(f'record size {record_size}, decoder expects {record.size}')
    need = HEADER.size + capacity * record_size
    if len(data) < need:
        raise SystemExit(f'short trace: {len(data)} bytes, expected at least {need}')

    first = max(0, write_index - capacity)
    rows = []
    for absolute in range(first, write_index):
        slot = absolute % capacity
        values = record.unpack_from(data, HEADER.size + slot * record_size)
        if version == 2:
            seq, flags, v69, v6a, v6b, polls, r66, status, counts = values
            tick_ms = run_count = state_pack = 0
        else:
            (seq, tick_ms, run_count, state_pack, flags, v69, v6a, v6b,
             polls, r66, status, counts) = values
        if seq != absolute:
            continue
        rows.append({
            'sequence': seq,
            'tick_ms': tick_ms,
            'run_count': run_count,
            'run_stop': state_pack & 0xFF,
            'flags': flags,
            'flags_text': flags_text(flags),
            'value_69': v69,
            'value_6a': v6a,
            'value_6b': v6b,
            'poll65_count': polls,
            'result_66': r66,
            'status_64': status & 0xFF,
            'final_65': (status >> 8) & 0xFF,
            'result_67': (status >> 16) & 0xFF,
            'bytes_69': counts & 0xFF,
            'bytes_6a': (counts >> 8) & 0xFF,
            'bytes_6b': (counts >> 16) & 0xFF,
            'bytes_66': (counts >> 24) & 0xFF,
        })
    meta = (version, capacity, write_index, wrapped, exports, dropped, record_size)
    return meta, rows

def main():
    ap = argparse.ArgumentParser(description='Decode DPOSDBG job traces v2/v3')
    ap.add_argument('trace')
    ap.add_argument('--csv', dest='csv_path')
    args = ap.parse_args()
    meta, rows = load(args.trace)
    version, capacity, written, wrapped, exports, dropped, rec_size = meta
    print(f'version={version} capacity={capacity} written={written} '
          f'wrapped={wrapped} exports={exports} dropped={dropped} '
          f'record_size={rec_size}')
    print('seq time_ms     d_ms mode run_count flags                         '
          'n6b value6b    st64 final65 polls r67 n66 result66')
    previous_tick = None
    for r in rows:
        tick = r['tick_ms']
        delta = 0 if previous_tick is None else ((tick - previous_tick) & 0xFFFFFFFF)
        previous_tick = tick
        mode = 'RUN' if r['run_stop'] == 0 else ('STOP' if r['run_stop'] == 1
                                                else f"0x{r['run_stop']:02X}")
        print(f"{r['sequence']:4d} {tick:10d} {delta:6d} {mode:<4} "
              f"{r['run_count']:9d} {r['flags_text']:<29} "
              f"{r['bytes_6b']:3d} 0x{r['value_6b']:08X} "
              f"0x{r['status_64']:02X} 0x{r['final_65']:02X} "
              f"{r['poll65_count']:5d} 0x{r['result_67']:02X} "
              f"{r['bytes_66']:3d} 0x{r['result_66']:08X}")
    if args.csv_path:
        fields = list(rows[0].keys()) if rows else [
            'sequence', 'tick_ms', 'run_count', 'run_stop']
        with open(args.csv_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)

if __name__ == '__main__':
    main()
