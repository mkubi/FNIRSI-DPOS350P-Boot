#!/usr/bin/env python3
"""Create a DEBUG EXE2 image with a local Portuguese-menu trap.

The input is the raw EXE2 payload loaded at 0x40000000. The script verifies
its known size, vector prefix, and original instruction before replacing the
Portuguese accepted-event instruction at 0x40055B68 with `b .`.
"""

import argparse
import hashlib
from pathlib import Path

EXE2_BASE = 0x40000000
EXPECTED_SIZE = 0x000957E0
PATCH_ADDRESS = 0x40055B68
PATCH_OFFSET = PATCH_ADDRESS - EXE2_BASE
EXPECTED_WORD = 0xE3A02000  # mov r2, #0
REPLACEMENT_WORD = 0xEAFFFFFE  # b .
EXPECTED_PREFIX = bytes.fromhex(
    "18 f0 9f e5 "
    "18 f0 9f e5 "
    "18 f0 9f e5 "
    "18 f0 9f e5 "
    "18 f0 9f e5 "
    "00 00 00 00 "
    "18 f0 9f e5 "
    "18 f0 9f e5"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="raw stock EXE2 payload")
    parser.add_argument("output", type=Path, help="patched EXE2DBG.BIN")
    args = parser.parse_args()

    source = args.input.read_bytes()

    if len(source) != EXPECTED_SIZE:
        raise SystemExit(
            f"ERROR: unexpected EXE2 size {len(source)} "
            f"(expected {EXPECTED_SIZE} / 0x{EXPECTED_SIZE:X})"
        )

    if source[: len(EXPECTED_PREFIX)] != EXPECTED_PREFIX:
        raise SystemExit("ERROR: EXE2 vector prefix does not match")

    original_word = int.from_bytes(
        source[PATCH_OFFSET : PATCH_OFFSET + 4], "little"
    )
    if original_word != EXPECTED_WORD:
        raise SystemExit(
            f"ERROR: word at 0x{PATCH_ADDRESS:08X} is 0x{original_word:08X}, "
            f"expected 0x{EXPECTED_WORD:08X}"
        )

    patched = bytearray(source)
    patched[PATCH_OFFSET : PATCH_OFFSET + 4] = REPLACEMENT_WORD.to_bytes(
        4, "little"
    )

    args.output.write_bytes(patched)

    readback = args.output.read_bytes()
    if readback != patched:
        raise SystemExit("ERROR: output readback mismatch")

    print(f"Input:       {args.input}")
    print(f"Output:      {args.output}")
    print(f"Size:        {len(readback)} bytes (0x{len(readback):X})")
    print(f"Patch VA:    0x{PATCH_ADDRESS:08X}")
    print(f"Patch offset:0x{PATCH_OFFSET:08X}")
    print(f"Original:    0x{EXPECTED_WORD:08X}")
    print(f"Replacement: 0x{REPLACEMENT_WORD:08X} (b .)")
    print(f"Input SHA256:  {sha256(source)}")
    print(f"Output SHA256: {sha256(readback)}")


if __name__ == "__main__":
    main()
