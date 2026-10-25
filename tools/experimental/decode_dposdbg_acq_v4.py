#!/usr/bin/env python3
import argparse
import csv
import struct
from pathlib import Path

MAGIC = 0x30435254
HEADER = struct.Struct('<8I')
RECORD = struct.Struct('<6I')
EVENT_NAMES = {0x1000: 'SELECT', 0x1001: 'WRITE', 0x1002: 'READ'}

def load(path):
    data = Path(path).read_bytes()
    if len(data) < HEADER.size:
        raise SystemExit('trace is shorter than the 32-byte header')
    magic, version, capacity, written, wrapped, exports, dropped, rec_size = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise SystemExit(f'bad magic 0x{magic:08X}, expected TRC0')
    if version != 4:
        raise SystemExit(f'unsupported trace version {version}, expected 4')
    if rec_size != RECORD.size:
        raise SystemExit(f'record size {rec_size}, decoder expects {RECORD.size}')
    need = HEADER.size + capacity * rec_size
    if len(data) < need:
        raise SystemExit(f'short trace: {len(data)} bytes, expected at least {need}')

    first = max(0, written - capacity)
    rows = []
    for absolute in range(first, written):
        slot = absolute % capacity
        seq, tick, run_count, event_state, value, call_site = RECORD.unpack_from(
            data, HEADER.size + slot * rec_size)
        if seq != absolute:
            continue
        event = event_state & 0xFFFF
        run_stop = (event_state >> 16) & 0xFF
        rows.append({
            'sequence': seq,
            'tick_ms': tick,
            'run_count': run_count,
            'run_stop': run_stop,
            'event': event,
            'operation': EVENT_NAMES.get(event, f'0x{event:04X}'),
            'value': value & 0xFF,
            'call_site': call_site,
        })
    return (version, capacity, written, wrapped, exports, dropped, rec_size), rows

def main():
    ap = argparse.ArgumentParser(description='Decode DPOSDBG acquisition primitive trace v4')
    ap.add_argument('trace')
    ap.add_argument('--csv', dest='csv_path')
    args = ap.parse_args()
    meta, rows = load(args.trace)
    version, capacity, written, wrapped, exports, dropped, rec_size = meta
    print(f'version={version} capacity={capacity} written={written} '
          f'wrapped={wrapped} exports={exports} dropped={dropped} '
          f'record_size={rec_size}')
    print('seq time_ms     d_ms mode run_count call_site operation value')
    previous_tick = None
    for r in rows:
        delta = 0 if previous_tick is None else ((r['tick_ms'] - previous_tick) & 0xFFFFFFFF)
        previous_tick = r['tick_ms']
        mode = 'RUN' if r['run_stop'] == 0 else ('STOP' if r['run_stop'] == 1
                                                else f"0x{r['run_stop']:02X}")
        print(f"{r['sequence']:4d} {r['tick_ms']:10d} {delta:6d} {mode:<4} "
              f"{r['run_count']:9d} 0x{r['call_site']:08X} "
              f"{r['operation']:<9} 0x{r['value']:02X}")
    if args.csv_path:
        fields = list(rows[0].keys()) if rows else [
            'sequence', 'tick_ms', 'run_count', 'run_stop',
            'event', 'operation', 'value', 'call_site']
        with open(args.csv_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)

if __name__ == '__main__':
    main()
