#!/usr/bin/env python3
import argparse
import struct
import subprocess
from pathlib import Path

BASE = 0x40000000
RANGES = [
    ("main", "arm", 0x40034700, 0x40034794),
    ("fpga_core", "arm", 0x40049094, 0x4004932C),
    ("gpio_helpers", "arm", 0x40063180, 0x400632C0),
    ("delay_gpio", "arm", 0x40066080, 0x40066110),
    ("bus_init", "arm", 0x4005F180, 0x4005F400),
    ("startup_arm", "arm", 0x4008CB28, 0x4008CBEC),
    ("startup_thumb", "thumb", 0x4008CBEC, 0x4008CC4C),
]

def objdump(binary: Path, mode: str, start: int, stop: int) -> str:
    machine = "arm"
    opts = "reg-names-std" if mode == "arm" else "force-thumb,reg-names-std"
    cmd = [
        "arm-none-eabi-objdump", "-D", "-b", "binary", "-m", machine,
        "-M", opts, f"--adjust-vma=0x{BASE:08x}",
        f"--start-address=0x{start:08x}", f"--stop-address=0x{stop:08x}",
        str(binary),
    ]
    return subprocess.run(cmd, check=True, text=True, capture_output=True).stdout

def main():
    ap = argparse.ArgumentParser(description="Extract DPOS350P EXE2 FPGA RE ranges")
    ap.add_argument("binary", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=Path("work/updater-re/fpga-re"))
    args = ap.parse_args()
    data = args.binary.read_bytes()
    args.output.mkdir(parents=True, exist_ok=True)

    index = []
    for name, mode, start, stop in RANGES:
        out = args.output / f"{name}-{mode}.lst"
        out.write_text(objdump(args.binary, mode, start, stop))
        index.append(f"{name:16s} {mode:5s} 0x{start:08X}..0x{stop:08X}  {out.name}")

    literal_start, literal_end = 0x4004932C, 0x4004934C
    lines = []
    for addr in range(literal_start, literal_end, 4):
        off = addr - BASE
        value = struct.unpack_from("<I", data, off)[0]
        lines.append(f"0x{addr:08X}: 0x{value:08X}")
    (args.output / "fpga-literals.txt").write_text("\n".join(lines) + "\n")

    summary = """Confirmed EXE2 anchors\n\nmain             0x40034700\nFPGA_Start       0x40049188\nFPGA ID probe    0x400492E4\nexpected magic   0xA5A55A5A\nPIO PE_CFG0      0x01C20890\nPIO PE_CFG1      0x01C20894\n\n"""
    (args.output / "README.txt").write_text(summary + "\n".join(index) + "\n")
    print(f"Wrote FPGA RE extracts to {args.output}")

if __name__ == "__main__":
    main()
