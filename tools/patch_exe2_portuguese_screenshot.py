#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path

EXE2_BASE = 0x40000000
EXPECTED_SIZE = 0x000957E0

PATCH_ADDRESS = 0x40055B68
PATCH_OFFSET = PATCH_ADDRESS - EXE2_BASE

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

EXPECTED_PATCH_BYTES = bytes.fromhex(
    "00 20 a0 e3 "  # mov  r2, #0
    "74 21 c1 e5 "  # strb r2, [r1, #0x174]
    "04 10 a0 e3 "  # mov  r1, #4
    "c8 0e 9f e5"   # ldr  r0, literal
)


def encode_arm_branch(site, target, link=False):
    displacement = target - (site + 8)

    if displacement & 3:
        raise ValueError("unaligned ARM branch target")

    opcode = 0xEB000000 if link else 0xEA000000
    return opcode | ((displacement >> 2) & 0x00FFFFFF)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = args.input.read_bytes()

    if len(source) != EXPECTED_SIZE:
        raise SystemExit(
            f"ERROR: size {len(source)}, expected {EXPECTED_SIZE}"
        )

    if source[:len(EXPECTED_PREFIX)] != EXPECTED_PREFIX:
        raise SystemExit("ERROR: EXE2 vector prefix does not match")

    actual = source[PATCH_OFFSET:PATCH_OFFSET + 16]

    if actual != EXPECTED_PATCH_BYTES:
        raise SystemExit(
            "ERROR: Portuguese path bytes do not match expected EXE2"
        )

    screenshot_bl = encode_arm_branch(
        0x40055B70,
        0x4003506C,
        link=True
    )

    epilogue_b = encode_arm_branch(
        0x40055B74,
        0x40055B90,
        link=False
    )

    replacement = (
        bytes.fromhex("00 20 a0 e3") +
        bytes.fromhex("74 21 c1 e5") +
        screenshot_bl.to_bytes(4, "little") +
        epilogue_b.to_bytes(4, "little")
    )

    patched = bytearray(source)
    patched[PATCH_OFFSET:PATCH_OFFSET + 16] = replacement

    args.output.write_bytes(patched)

    readback = args.output.read_bytes()

    if readback != patched:
        raise SystemExit("ERROR: output readback mismatch")

    print(f"Input:        {args.input}")
    print(f"Output:       {args.output}")
    print(f"Size:         {len(readback)} bytes (0x{len(readback):X})")
    print(f"Patch VA:     0x{PATCH_ADDRESS:08X}")
    print(f"Patch offset: 0x{PATCH_OFFSET:08X}")
    print()
    print("Patched flow:")
    print("  0x40055B68: MOV  r2, #0")
    print("  0x40055B6C: STRB r2, [r1, #0x174]")
    print("  0x40055B70: BL   0x4003506C")
    print("  0x40055B74: B    0x40055B90")
    print()
    print(f"Input SHA256:  {sha256(source)}")
    print(f"Output SHA256: {sha256(readback)}")


if __name__ == "__main__":
    main()
