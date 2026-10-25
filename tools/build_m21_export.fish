#!/usr/bin/env fish
set -e
cd ~/FNIRSI/FNIRSI-DPOS350P-Boot

arm-none-eabi-gcc -c -mcpu=cortex-a7 -marm -ffreestanding -nostdlib \
    tools/portuguese_m21_export.S -o /tmp/portuguese_m21_export.o

arm-none-eabi-ld -T tools/portuguese_m21_export.ld \
    /tmp/portuguese_m21_export.o -o /tmp/portuguese_m21_export.elf

arm-none-eabi-objcopy -O binary --only-section=.text \
    /tmp/portuguese_m21_export.elf /tmp/portuguese_m21_export.bin

arm-none-eabi-objdump -d /tmp/portuguese_m21_export.elf

python3 -c '
from pathlib import Path
import hashlib

source_path = Path("work/updater-re/exe2-from-backup.bin")
blob_path = Path("/tmp/portuguese_m21_export.bin")
output_path = Path.home() / "Downloads" / "EXE2DBG.BIN"

expected_hash = "8eeb56ba00865d7bacde2d56c006c57cc8284d22bd26b24fcf0e3cf4defbca19"
source = source_path.read_bytes()
blob = blob_path.read_bytes()

if hashlib.sha256(source).hexdigest() != expected_hash:
    raise SystemExit("ERROR: unexpected stock EXE2 SHA256")
if len(source) != 0x957E0:
    raise SystemExit(f"ERROR: unexpected EXE2 size 0x{len(source):X}")

print(f"M2.1 blob size: {len(blob)} bytes / 0x{len(blob):X}")
if len(blob) > 0xC4:
    raise SystemExit(f"ERROR: M2.1 size 0x{len(blob):X} exceeds 0xC4")

patched = bytearray(source)
patched[0x55AD4:0x55AD4 + len(blob)] = blob
output_path.write_bytes(patched)

if output_path.read_bytes() != patched:
    raise SystemExit("ERROR: EXE2DBG readback mismatch")

print(f"Output:        {output_path}")
print(f"Output size:   {len(patched)} bytes / 0x{len(patched):X}")
print(f"Output SHA256: {hashlib.sha256(patched).hexdigest()}")
'
