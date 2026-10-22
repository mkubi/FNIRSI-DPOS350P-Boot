#!/usr/bin/env python3
import argparse
from pathlib import Path
import struct

MAGIC = b"eGON.BT0"
STAMP = 0x5F0A6C39
HEADER_SIZE = 0x20
DEFAULT_LENGTH = 0x6800
CHECKSUM_OFFSET = 0x0C
LENGTH_OFFSET = 0x10
MARKER = b"SPL\x02"

def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]

def put_u32(data, offset, value):
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)

def checksum_with_stamp(image):
    work = bytearray(image)
    put_u32(work, CHECKSUM_OFFSET, STAMP)
    total = 0
    for offset in range(0, len(work), 4):
        total = (total + u32(work, offset)) & 0xFFFFFFFF
    return total

def decode_branch_target(instruction):
    if (instruction & 0xFF000000) != 0xEA000000:
        raise ValueError(f"first word is not unconditional ARM B: 0x{instruction:08X}")
    imm24 = instruction & 0x00FFFFFF
    if imm24 & 0x00800000:
        imm24 -= 0x01000000
    return 8 + imm24 * 4

def validate(image):
    errors = []
    if len(image) < HEADER_SIZE:
        return ["image is shorter than the 32-byte header"]
    if len(image) & 3:
        errors.append("image size is not 4-byte aligned")
    if image[4:12] != MAGIC:
        errors.append(f"bad magic: {image[4:12]!r}")
    if image[0x14:0x18] != MARKER:
        errors.append(f"bad SPL marker: {image[0x14:0x18]!r}")
    declared = u32(image, LENGTH_OFFSET)
    if declared != len(image):
        errors.append(f"declared length 0x{declared:X} != file size 0x{len(image):X}")
    try:
        target = decode_branch_target(u32(image, 0))
        if target != HEADER_SIZE:
            errors.append(f"branch target 0x{target:X} != 0x{HEADER_SIZE:X}")
    except ValueError as exc:
        errors.append(str(exc))
    stored = u32(image, CHECKSUM_OFFSET)
    calculated = checksum_with_stamp(image)
    if stored != calculated:
        errors.append(f"checksum mismatch: stored 0x{stored:08X}, calculated 0x{calculated:08X}")
    return errors

def main():
    parser = argparse.ArgumentParser(description="Pack/verify compact DPOS350P eGON.BT0 image")
    parser.add_argument("input", type=Path, help="raw objcopy binary linked at SRAM address 0")
    parser.add_argument("output", type=Path, nargs="?", help="final padded/checksummed BT0 image")
    parser.add_argument("--length", type=lambda value: int(value, 0), default=DEFAULT_LENGTH)
    parser.add_argument("--verify", action="store_true", help="verify input as an already packed BT0 image")
    args = parser.parse_args()

    data = bytearray(args.input.read_bytes())

    if args.verify:
        errors = validate(data)
        if errors:
            for error in errors:
                print("ERROR:", error)
            raise SystemExit(1)
        print(f"valid BT0       : {args.input}")
        print(f"length          : 0x{len(data):X} ({len(data)} bytes)")
        print(f"branch target   : 0x{decode_branch_target(u32(data, 0)):X}")
        print(f"checksum        : 0x{u32(data, CHECKSUM_OFFSET):08X}")
        return

    if args.output is None:
        parser.error("output is required unless --verify is used")
    if len(data) > args.length:
        raise SystemExit(f"raw image too large: 0x{len(data):X} > 0x{args.length:X}")
    if args.length & 0x1FF:
        raise SystemExit("BT0 length must be 512-byte aligned")
    if len(data) < HEADER_SIZE:
        raise SystemExit("raw image does not contain the embedded BT0 header")
    if data[4:12] != MAGIC:
        raise SystemExit(f"raw image has wrong magic: {data[4:12]!r}")
    if decode_branch_target(u32(data, 0)) != HEADER_SIZE:
        raise SystemExit("embedded first branch does not target offset 0x20")

    data.extend(b"\x00" * (args.length - len(data)))
    put_u32(data, LENGTH_OFFSET, args.length)
    put_u32(data, CHECKSUM_OFFSET, STAMP)
    put_u32(data, CHECKSUM_OFFSET, checksum_with_stamp(data))

    errors = validate(data)
    if errors:
        raise SystemExit("internal validation failed: " + "; ".join(errors))

    args.output.write_bytes(data)
    print(f"raw image       : {args.input}")
    print(f"raw size        : 0x{len(args.input.read_bytes()):X}")
    print(f"BT0 image       : {args.output}")
    print(f"BT0 length      : 0x{len(data):X} ({len(data)} bytes)")
    print(f"branch target   : 0x{decode_branch_target(u32(data, 0)):X}")
    print(f"checksum        : 0x{u32(data, CHECKSUM_OFFSET):08X}")

if __name__ == "__main__":
    main()
