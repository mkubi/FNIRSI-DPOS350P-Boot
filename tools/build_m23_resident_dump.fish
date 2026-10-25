#!/usr/bin/env fish

cd ~/FNIRSI/FNIRSI-DPOS350P-Boot; or exit 1

python3 -c '
from pathlib import Path

Path("tools/portuguese_m23_resident_dump.S").write_text(""".syntax unified
.cpu cortex-a7
.arm

.equ EVENT_STRUCTURE,    0x42E61FF8
.equ RESIDENT_SOURCE,    0x42E70000
.equ STOCK_EVENT_UPDATE, 0x4002FD80
.equ STOCK_F_OPEN,       0x40059B8C
.equ STOCK_F_WRITE,      0x4005A1D4
.equ STOCK_F_CLOSE,      0x4005A694
.equ FRAME_SIZE,         0x254
.equ DUMP_SIZE,          0x40

.section .text, "ax", %progbits
.global portuguese_m23_resident_dump
.type portuguese_m23_resident_dump, %function

portuguese_m23_resident_dump:
    push    {r4, lr}
    sub     sp, sp, #FRAME_SIZE

    bl      STOCK_EVENT_UPDATE

    ldr     r4, =EVENT_STRUCTURE
    ldrb    r0, [r4, #0x174]
    cmp     r0, #2
    bne     return_to_caller

    mov     r0, #0
    strb    r0, [r4, #0x174]

    mov     r0, sp
    adr     r1, output_path
    mov     r2, #0x0A
    bl      STOCK_F_OPEN
    cmp     r0, #0
    bne     return_to_caller

    mov     r0, sp
    ldr     r1, =RESIDENT_SOURCE
    mov     r2, #DUMP_SIZE
    mov     r3, #0
    bl      STOCK_F_WRITE

    mov     r0, sp
    bl      STOCK_F_CLOSE

return_to_caller:
    add     sp, sp, #FRAME_SIZE
    pop     {r4, pc}

.align 2
output_path:
    .asciz  "/DPOSRES.BIN"

.size portuguese_m23_resident_dump, . - portuguese_m23_resident_dump
.ltorg
""")

Path("tools/portuguese_m23_resident_dump.ld").write_text("""ENTRY(portuguese_m23_resident_dump)

SECTIONS
{
    . = 0x40055AD4;

    .text : ALIGN(4)
    {
        *(.text*)
    }

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
        *(.ARM.attributes*)
    }
}
""")

print("M2.3 sources created")
'; or exit 1

arm-none-eabi-gcc \
    -c \
    -mcpu=cortex-a7 \
    -marm \
    -ffreestanding \
    -nostdlib \
    tools/portuguese_m23_resident_dump.S \
    -o /tmp/portuguese_m23_resident_dump.o; or exit 1

arm-none-eabi-ld \
    -T tools/portuguese_m23_resident_dump.ld \
    /tmp/portuguese_m23_resident_dump.o \
    -o /tmp/portuguese_m23_resident_dump.elf; or exit 1

arm-none-eabi-objcopy \
    -O binary \
    --only-section=.text \
    /tmp/portuguese_m23_resident_dump.elf \
    /tmp/portuguese_m23_resident_dump.bin; or exit 1

arm-none-eabi-objdump -d \
    /tmp/portuguese_m23_resident_dump.elf; or exit 1

python3 -c '
from pathlib import Path
import hashlib

source_path = Path("work/updater-re/exe2-from-backup.bin")
blob_path = Path("/tmp/portuguese_m23_resident_dump.bin")
output_path = Path.home() / "Downloads" / "EXE2DBG.BIN"

expected_hash = "8eeb56ba00865d7bacde2d56c006c57cc8284d22bd26b24fcf0e3cf4defbca19"
patch_offset = 0x55AD4
handler_size = 0xC4
expected_size = 0x957E0

source = source_path.read_bytes()
blob = blob_path.read_bytes()
actual_hash = hashlib.sha256(source).hexdigest()

if actual_hash != expected_hash:
    raise SystemExit(f"ERROR: unexpected source SHA256: {actual_hash}")
if len(source) != expected_size:
    raise SystemExit(f"ERROR: unexpected EXE2 size: 0x{len(source):X}")

print(f"M2.3 blob size: {len(blob)} bytes / 0x{len(blob):X}")
if len(blob) > handler_size:
    raise SystemExit(
        f"ERROR: M2.3 size 0x{len(blob):X} exceeds 0x{handler_size:X}"
    )

patched = bytearray(source)
patched[patch_offset:patch_offset + len(blob)] = blob
output_path.write_bytes(patched)
readback = output_path.read_bytes()

if readback != patched:
    raise SystemExit("ERROR: EXE2DBG readback mismatch")

print(f"Output:        {output_path}")
print(f"Output size:   {len(readback)} bytes / 0x{len(readback):X}")
print(f"Patch VA:      0x{0x40000000 + patch_offset:08X}")
print(f"Output SHA256: {hashlib.sha256(readback).hexdigest()}")
print("Expected dump: /DPOSRES.BIN, 64 bytes")
'; or exit 1
