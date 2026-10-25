#!/usr/bin/env python3
"""Build a DPOS350P update ISO containing only the package header and EXE2.

The original file is used as the trusted layout source. The complete BMP/padding
region [0x1000, 0x51000) is zeroed, EXE2 is preserved from 0x51000 up to the
36-byte footer, and the FNIRSI 9-word footer is recalculated.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

PACKAGE_MAGIC = b"DPOS350P"
HEADER_SIZE = 0x1000
EXE2_OFFSET = 0x51000
FOOTER_SIZE = 36
EGON_EXE_MAGIC_OFFSET = EXE2_OFFSET + 4
EGON_EXE_MAGIC = b"eGON.EXE"


def footer_words(payload: bytes) -> tuple[int, ...]:
    """Return the FNIRSI footer: byte sum followed by bit-position sums 7..0."""
    sums = [0] * 9
    for index, value in enumerate(payload):
        sums[0] = (sums[0] + value) & 0xFFFFFFFF
        for bit in range(7, -1, -1):
            if value & (1 << bit):
                footer_index = 1 + (7 - bit)
                sums[footer_index] = (sums[footer_index] + index) & 0xFFFFFFFF
    return tuple(sums)


def decode_footer(data: bytes) -> tuple[int, ...]:
    if len(data) < FOOTER_SIZE:
        raise ValueError("file is shorter than the 36-byte footer")
    return struct.unpack("<9I", data[-FOOTER_SIZE:])


def validate_layout(data: bytes) -> None:
    if len(data) <= EXE2_OFFSET + FOOTER_SIZE:
        raise ValueError("file is too short to contain EXE2 and footer")
    if data[:8] != PACKAGE_MAGIC:
        raise ValueError(f"bad package magic: {data[:8]!r}")
    if data[EGON_EXE_MAGIC_OFFSET:EGON_EXE_MAGIC_OFFSET + 8] != EGON_EXE_MAGIC:
        found = data[EGON_EXE_MAGIC_OFFSET:EGON_EXE_MAGIC_OFFSET + 8]
        raise ValueError(f"EXE2 magic not found at 0x{EXE2_OFFSET:X}: {found!r}")


def verify_footer(data: bytes) -> bool:
    expected = decode_footer(data)
    calculated = footer_words(data[:-FOOTER_SIZE])
    return expected == calculated


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create a minimal DPOS350P ISO with BMP removed and EXE2 retained"
    )
    parser.add_argument("source", type=Path, help="known-good original FNIRSI ISO")
    parser.add_argument("output", type=Path, help="minimal EXE2-only test ISO")
    parser.add_argument(
        "--allow-invalid-source-footer",
        action="store_true",
        help="continue even if the source footer does not verify",
    )
    args = parser.parse_args()

    source = args.source.read_bytes()
    validate_layout(source)

    source_footer_ok = verify_footer(source)
    if not source_footer_ok and not args.allow_invalid_source_footer:
        raise SystemExit(
            "source footer verification failed; refusing to build without "
            "--allow-invalid-source-footer"
        )

    output = bytearray(source)

    # Keep the 4 KiB DPOS350P package header. Remove eGON.BMP and all data before EXE2.
    output[HEADER_SIZE:EXE2_OFFSET] = b"\x00" * (EXE2_OFFSET - HEADER_SIZE)

    # Recalculate the 36-byte, nine-word FNIRSI footer.
    new_footer = footer_words(output[:-FOOTER_SIZE])
    output[-FOOTER_SIZE:] = struct.pack("<9I", *new_footer)

    validate_layout(output)
    if not verify_footer(output):
        raise SystemExit("internal error: generated footer does not verify")

    args.output.write_bytes(output)

    zero_region = output[HEADER_SIZE:EXE2_OFFSET]
    print(f"source                 : {args.source}")
    print(f"source size            : 0x{len(source):X} ({len(source)} bytes)")
    print(f"source SHA-256         : {sha256(source)}")
    print(f"source footer valid    : {source_footer_ok}")
    print(f"output                 : {args.output}")
    print(f"output size            : 0x{len(output):X} ({len(output)} bytes)")
    print(f"output SHA-256         : {sha256(output)}")
    print(f"zeroed range           : 0x{HEADER_SIZE:06X}..0x{EXE2_OFFSET - 1:06X}")
    print(f"zeroed bytes           : {len(zero_region)}")
    print(f"zero region all zero   : {all(value == 0 for value in zero_region)}")
    print(f"EXE2 magic             : {output[EGON_EXE_MAGIC_OFFSET:EGON_EXE_MAGIC_OFFSET + 8]!r}")
    print(f"new footer words       : {' '.join(f'{x:08X}' for x in new_footer)}")
    print(f"output footer valid    : {verify_footer(output)}")


if __name__ == "__main__":
    main()
