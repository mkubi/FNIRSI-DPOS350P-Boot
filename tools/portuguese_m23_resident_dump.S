.syntax unified
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
