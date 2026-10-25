#!/usr/bin/env fish
cd ~/FNIRSI/FNIRSI-DPOS350P-Boot; or exit 1

arm-none-eabi-gcc -c -mcpu=cortex-a7 -marm -ffreestanding -nostdlib \
    tools/portuguese_m22_dump.S -o /tmp/portuguese_m22_dump.o; or exit 1
arm-none-eabi-ld -T tools/portuguese_m22_dump.ld \
    /tmp/portuguese_m22_dump.o -o /tmp/portuguese_m22_dump.elf; or exit 1
arm-none-eabi-objcopy -O binary --only-section=.text \
    /tmp/portuguese_m22_dump.elf /tmp/portuguese_m22_dump.bin; or exit 1
arm-none-eabi-objdump -d /tmp/portuguese_m22_dump.elf; or exit 1

python3 -c '
from pathlib import Path
import hashlib
source_path = Path("work/updater-re/exe2-from-backup.bin")
blob_path = Path("/tmp/portuguese_m22_dump.bin")
output_path = Path.home() / "Downloads" / "EXE2DBG.BIN"
expected_hash = "8eeb56ba00865d7bacde2d56c006c57cc8284d22bd26b24fcf0e3cf4defbca19"
source = source_path.read_bytes()
blob = blob_path.read_bytes()
if hashlib.sha256(source).hexdigest() != expected_hash:
    raise SystemExit("ERROR: unexpected stock EXE2 SHA256")
if len(source) != 0x957E0:
    raise SystemExit(f"ERROR: unexpected EXE2 size 0x{len(source):X}")
print(f"M2.2 blob size: {len(blob)} bytes / 0x{len(blob):X}")
if len(blob) > 0xC4:
    raise SystemExit(f"ERROR: M2.2 size 0x{len(blob):X} exceeds 0xC4")
patched = bytearray(source)
patched[0x55AD4:0x55AD4 + len(blob)] = blob
output_path.write_bytes(patched)
if output_path.read_bytes() != patched:
    raise SystemExit("ERROR: EXE2DBG readback mismatch")
print(f"Output:        {output_path}")
print(f"Output size:   {len(patched)} bytes / 0x{len(patched):X}")
print(f"Output SHA256: {hashlib.sha256(patched).hexdigest()}")
'; or exit 1
