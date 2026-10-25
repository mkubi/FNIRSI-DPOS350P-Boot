#!/usr/bin/env python3
import struct
import sys
from pathlib import Path

def kind(value):
    return {
        0: "fault/unmapped",
        1: "coarse page table",
        2: "section/supersection",
        3: "reserved",
    }[value & 3]

path = Path(sys.argv[1] if len(sys.argv) > 1 else Path.home() / "Downloads" / "DPOSMMU.BIN")
data = path.read_bytes()
if len(data) != 16:
    raise SystemExit(f"ERROR: expected 16 bytes, got {len(data)}")
magic, d400, d410, d417 = struct.unpack("<4I", data)
if magic != 0x3132444D:
    raise SystemExit(f"ERROR: bad magic 0x{magic:08X}")
print(f"VA 0x40000000: 0x{d400:08X}  {kind(d400)}")
print(f"VA 0x41000000: 0x{d410:08X}  {kind(d410)}")
print(f"VA 0x41700000: 0x{d417:08X}  {kind(d417)}")
