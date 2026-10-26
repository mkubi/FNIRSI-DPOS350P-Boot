#!/usr/bin/env python3
"""Decode DPOSLAB resident runtime trace files.

Expected DPOSLAB trace ABI v1:

    trace header:  0x20 bytes
    trace record:  0x84 bytes
    snapshot:      0x74 bytes

The decoder validates the container, prints every checkpoint and produces
field-by-field differences between adjacent records. It can also export CSV
or JSON for later analysis.
"""

from __future__ import annotations

import argparse
import csv
import json
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Sequence

TRACE_MAGIC = 0x42414C44  # "DLAB" in a little-endian file
TRACE_VERSION = 1
TRACE_HEADER_SIZE = 0x20
TRACE_RECORD_SIZE = 0x84
SNAPSHOT_SIZE = 0x74

HEADER_STRUCT = struct.Struct("<8I")
RECORD_PREFIX_STRUCT = struct.Struct("<4I")
SNAPSHOT_STRUCT = struct.Struct("<29I")

CHECKPOINT_NAMES = {
    0x00: "NONE",
    0x01: "FPGA_START_ENTRY",
    0x02: "FPGA_START_RETURN",
    0x0F: "FPGA_EARLY_INIT",
    0x10: "FPGA_PHASE_A",
    0x11: "FPGA_PHASE_B",
    0x12: "FPGA_CLOCK_95M",
    0x13: "FPGA_CLOCK_10M",
    0x14: "FPGA_PHASE_C",
    0x20: "CSI_AFTER_CLOCK_RESET",
    0x21: "CSI_AFTER_BASE_BIND",
    0x22: "CSI_AFTER_ENABLE_A",
    0x23: "CSI_AFTER_ENABLE_B",
    0x24: "CSI_AFTER_CONFIG_8000",
    0x25: "CSI_AFTER_PINMUX",
}

REGISTER_NAMES = [f"r{i}" for i in range(13)]
SNAPSHOT_FIELD_NAMES = (
    REGISTER_NAMES
    + [
        "lr",
        "cpsr",
        "stock_sp",
        "exec_sp",
        "pe_cfg0",
        "pe_cfg1",
        "pe_data",
        "pe_pull0",
        "ccu_gate0",
        "ccu_reset0",
        "csi_if_cfg",
        "csi_mode",
        "csi_hsize",
        "csi_destination",
        "csi_irq_enable",
        "csi_status",
    ]
)

DISPLAY_GROUPS = (
    ("CPU", REGISTER_NAMES + ["lr", "cpsr", "stock_sp", "exec_sp"]),
    ("PE", ["pe_cfg0", "pe_cfg1", "pe_data", "pe_pull0"]),
    ("CCU", ["ccu_gate0", "ccu_reset0"]),
    (
        "CSI",
        [
            "csi_if_cfg",
            "csi_mode",
            "csi_hsize",
            "csi_destination",
            "csi_irq_enable",
            "csi_status",
        ],
    ),
)


class TraceError(ValueError):
    """Raised when a trace file violates the DPOSLAB trace ABI."""


@dataclass(frozen=True)
class TraceHeader:
    magic: int
    version: int
    header_size: int
    record_size: int
    capacity: int
    record_count: int
    dropped: int
    flags: int


@dataclass(frozen=True)
class TraceRecord:
    sequence: int
    checkpoint: int
    execution_address: int
    reserved: int
    snapshot: dict[str, int]

    @property
    def checkpoint_name(self) -> str:
        return CHECKPOINT_NAMES.get(
            self.checkpoint, f"UNKNOWN_0x{self.checkpoint:08X}"
        )


def hex32(value: int) -> str:
    return f"0x{value & 0xFFFFFFFF:08X}"


def parse_trace(data: bytes, strict_size: bool = False) -> tuple[TraceHeader, list[TraceRecord]]:
    if len(data) < HEADER_STRUCT.size:
        raise TraceError(
            f"file is too short: {len(data)} bytes, need at least {HEADER_STRUCT.size}"
        )

    header = TraceHeader(*HEADER_STRUCT.unpack_from(data, 0))

    if header.magic != TRACE_MAGIC:
        raw = data[:4]
        raise TraceError(
            f"bad magic {hex32(header.magic)} (raw {raw!r}), expected DLAB/{hex32(TRACE_MAGIC)}"
        )
    if header.version != TRACE_VERSION:
        raise TraceError(
            f"unsupported trace version {header.version}, expected {TRACE_VERSION}"
        )
    if header.header_size != TRACE_HEADER_SIZE:
        raise TraceError(
            f"unexpected header size {hex32(header.header_size)}, expected {hex32(TRACE_HEADER_SIZE)}"
        )
    if header.record_size != TRACE_RECORD_SIZE:
        raise TraceError(
            f"unexpected record size {hex32(header.record_size)}, expected {hex32(TRACE_RECORD_SIZE)}"
        )
    if header.capacity == 0:
        raise TraceError("trace capacity is zero")
    if header.record_count > header.capacity:
        raise TraceError(
            f"record_count {header.record_count} exceeds capacity {header.capacity}"
        )

    expected_size = header.header_size + header.record_count * header.record_size
    if len(data) < expected_size:
        raise TraceError(
            f"truncated trace: {len(data)} bytes, expected at least {expected_size}"
        )
    if strict_size and len(data) != expected_size:
        raise TraceError(
            f"unexpected trailing data: file has {len(data)} bytes, valid trace uses {expected_size}"
        )

    records: list[TraceRecord] = []
    for index in range(header.record_count):
        offset = header.header_size + index * header.record_size
        sequence, checkpoint, address, reserved = RECORD_PREFIX_STRUCT.unpack_from(
            data, offset
        )
        snapshot_offset = offset + RECORD_PREFIX_STRUCT.size
        values = SNAPSHOT_STRUCT.unpack_from(data, snapshot_offset)
        snapshot = dict(zip(SNAPSHOT_FIELD_NAMES, values, strict=True))
        records.append(
            TraceRecord(sequence, checkpoint, address, reserved, snapshot)
        )

    return header, records


def changed_fields(before: TraceRecord, after: TraceRecord) -> list[str]:
    return [
        name
        for name in SNAPSHOT_FIELD_NAMES
        if before.snapshot[name] != after.snapshot[name]
    ]


def gpr_changed_mask(before: TraceRecord, after: TraceRecord) -> int:
    mask = 0
    for index, name in enumerate(REGISTER_NAMES):
        if before.snapshot[name] != after.snapshot[name]:
            mask |= 1 << index
    return mask


def print_header(path: Path, file_size: int, header: TraceHeader) -> None:
    valid_size = header.header_size + header.record_count * header.record_size
    print("DPOSLAB TRACE")
    print("=============")
    print(f"file:          {path}")
    print(f"file size:     {file_size} bytes ({hex32(file_size)})")
    print(f"valid size:    {valid_size} bytes ({hex32(valid_size)})")
    print(f"magic:         {hex32(header.magic)} ('DLAB')")
    print(f"version:       {header.version}")
    print(f"header size:   {header.header_size} bytes ({hex32(header.header_size)})")
    print(f"record size:   {header.record_size} bytes ({hex32(header.record_size)})")
    print(f"capacity:      {header.capacity}")
    print(f"record count:  {header.record_count}")
    print(f"dropped:       {header.dropped}")
    print(f"flags:         {hex32(header.flags)}")
    if file_size > valid_size:
        print(f"trailing data: {file_size - valid_size} bytes")
    print()


def print_records(records: Sequence[TraceRecord], compact: bool = False) -> None:
    for record in records:
        print(
            f"[{record.sequence:02d}] {record.checkpoint_name} "
            f"@ {hex32(record.execution_address)}"
        )
        print(f"     checkpoint: {hex32(record.checkpoint)}")
        if record.reserved:
            print(f"     reserved:   {hex32(record.reserved)}")

        for group_name, names in DISPLAY_GROUPS:
            if compact and group_name == "CPU":
                names = ["lr", "cpsr", "stock_sp", "exec_sp"]
            print(f"     {group_name}:")
            for name in names:
                print(f"       {name:<17} {hex32(record.snapshot[name])}")
        print()


def print_diffs(records: Sequence[TraceRecord], include_unchanged: bool = False) -> None:
    if len(records) < 2:
        print("No adjacent-record diff available.")
        return

    print("ADJACENT CHECKPOINT DIFFS")
    print("=========================")

    for before, after in zip(records, records[1:]):
        fields = changed_fields(before, after)
        print(
            f"[{before.sequence:02d} -> {after.sequence:02d}] "
            f"{before.checkpoint_name} -> {after.checkpoint_name}"
        )
        print(
            f"  addresses: {hex32(before.execution_address)} -> "
            f"{hex32(after.execution_address)}"
        )
        print(f"  GPR changed mask: {hex32(gpr_changed_mask(before, after))}")

        if not fields:
            print("  no snapshot fields changed")
            print()
            continue

        for group_name, names in DISPLAY_GROUPS:
            selected = [
                name
                for name in names
                if include_unchanged or name in fields
            ]
            if not selected:
                continue
            print(f"  {group_name}:")
            for name in selected:
                old = before.snapshot[name]
                new = after.snapshot[name]
                marker = "*" if old != new else " "
                print(f"   {marker} {name:<17} {hex32(old)} -> {hex32(new)}")
        print()


def flat_record(record: TraceRecord) -> dict[str, object]:
    result: dict[str, object] = {
        "sequence": record.sequence,
        "checkpoint": record.checkpoint,
        "checkpoint_name": record.checkpoint_name,
        "execution_address": hex32(record.execution_address),
        "reserved": hex32(record.reserved),
    }
    result.update({name: hex32(value) for name, value in record.snapshot.items()})
    return result


def export_csv(path: Path, records: Sequence[TraceRecord]) -> None:
    rows = [flat_record(record) for record in records]
    fieldnames = [
        "sequence",
        "checkpoint",
        "checkpoint_name",
        "execution_address",
        "reserved",
        *SNAPSHOT_FIELD_NAMES,
    ]
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def export_json(path: Path, header: TraceHeader, records: Sequence[TraceRecord]) -> None:
    payload = {
        "header": {
            key: hex32(value) if key in {"magic", "flags"} else value
            for key, value in asdict(header).items()
        },
        "records": [flat_record(record) for record in records],
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Decode a DPOSLAB.TRC runtime trace file."
    )
    parser.add_argument("trace", type=Path, help="path to DPOSLAB.TRC")
    parser.add_argument(
        "--compact",
        action="store_true",
        help="omit r0-r12 from the per-record listing",
    )
    parser.add_argument(
        "--no-records",
        action="store_true",
        help="do not print the complete per-record listing",
    )
    parser.add_argument(
        "--no-diff",
        action="store_true",
        help="do not print adjacent checkpoint differences",
    )
    parser.add_argument(
        "--show-unchanged",
        action="store_true",
        help="include unchanged fields in adjacent-record diffs",
    )
    parser.add_argument(
        "--strict-size",
        action="store_true",
        help="reject files containing bytes after the valid records",
    )
    parser.add_argument("--csv", type=Path, help="write decoded records as CSV")
    parser.add_argument("--json", type=Path, help="write decoded trace as JSON")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    try:
        data = args.trace.read_bytes()
        header, records = parse_trace(data, strict_size=args.strict_size)
    except OSError as error:
        print(f"error: cannot read {args.trace}: {error}", file=sys.stderr)
        return 1
    except TraceError as error:
        print(f"error: invalid DPOSLAB trace: {error}", file=sys.stderr)
        return 2

    print_header(args.trace, len(data), header)

    if not args.no_records:
        print_records(records, compact=args.compact)

    if not args.no_diff:
        print_diffs(records, include_unchanged=args.show_unchanged)

    try:
        if args.csv:
            export_csv(args.csv, records)
            print(f"CSV written:  {args.csv}")
        if args.json:
            export_json(args.json, header, records)
            print(f"JSON written: {args.json}")
    except OSError as error:
        print(f"error: cannot write output: {error}", file=sys.stderr)
        return 3

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
