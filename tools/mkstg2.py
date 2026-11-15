#!/usr/bin/env python3
import argparse
import binascii
from pathlib import Path
import struct

MAGIC = 0x32475453
HEADER_VERSION = 1
VERSION_HEADER = Path(__file__).resolve().parents[1] / "loader" / "stage2_version.h"
version_text = VERSION_HEADER.read_text()
version_match = __import__("re").search(
    r"^#define\s+STAGE2_IMAGE_VERSION\s+(0x[0-9A-Fa-f]+|[0-9]+)U?\s*$",
    version_text,
    __import__("re").MULTILINE,
)
if not version_match:
    raise SystemExit(f"cannot parse STAGE2_IMAGE_VERSION from {VERSION_HEADER}")
IMAGE_VERSION = int(version_match.group(1), 0)
HEADER_SIZE = 32
SLOT_SIZE = 0x10000
LOAD_ADDRESS = 0x41000000

parser = argparse.ArgumentParser(description="Build DPOS350P STG2 NOR slot image")
parser.add_argument("payload", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--entry-offset", type=lambda value: int(value, 0), default=0)
args = parser.parse_args()

payload = args.payload.read_bytes()
if not payload:
    raise SystemExit("payload is empty")
if len(payload) > SLOT_SIZE - HEADER_SIZE:
    raise SystemExit(f"payload too large: {len(payload)} > {SLOT_SIZE - HEADER_SIZE}")
if args.entry_offset < 0 or args.entry_offset >= len(payload):
    raise SystemExit("entry offset is outside payload")

entry = LOAD_ADDRESS + args.entry_offset
crc = binascii.crc32(payload) & 0xffffffff
header = struct.pack("<8I", MAGIC, HEADER_VERSION, HEADER_SIZE, len(payload),
                     LOAD_ADDRESS, entry, crc, IMAGE_VERSION)
slot = header + payload
slot += b"\xff" * (SLOT_SIZE - len(slot))
args.output.write_bytes(slot)

print(f"payload       : {args.payload}")
print(f"payload size  : 0x{len(payload):X} ({len(payload)} bytes)")
print(f"load address  : 0x{LOAD_ADDRESS:08X}")
print(f"entry address : 0x{entry:08X}")
print(f"CRC32         : 0x{crc:08X}")
print(f"image version : {IMAGE_VERSION}")
print(f"slot image    : {args.output}")
print(f"slot size     : 0x{len(slot):X} ({len(slot)} bytes)")
