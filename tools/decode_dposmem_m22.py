#!/usr/bin/env python3
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1] if len(sys.argv) > 1 else Path.home() / "Downloads" / "DPOSMEM.BIN")
data = path.read_bytes()
if len(data) != 0x60:
    raise SystemExit(f"ERROR: expected 96 bytes, got {len(data)}")
block410 = data[:0x20]
block417 = data[0x20:0x60]
print(f"File: {path}")
print("0x41003C40:", block410.hex(" "))
print("0x41700000:", block417.hex(" "))
print(f"0x41003C48 word: 0x{struct.unpack_from('<I', block410, 0x08)[0]:08X}")
print(f"0x41700000 word: 0x{struct.unpack_from('<I', block417, 0x00)[0]:08X}")
print(f"0x41700020 word: 0x{struct.unpack_from('<I', block417, 0x20)[0]:08X}")
