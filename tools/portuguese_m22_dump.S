.syntax unified
.cpu cortex-a7
.arm

.equ EVENT_STRUCTURE,    0x42E61FF8
.equ STOCK_EVENT_UPDATE, 0x4002FD80
.equ STOCK_F_OPEN,       0x40059B8C
.equ STOCK_F_WRITE,      0x4005A1D4
.equ STOCK_F_CLOSE,      0x4005A694
.equ SOURCE_410,         0x41003C40
.equ SOURCE_417,         0x41700000
.equ FRAME_SIZE,         0x254

.section .text, "ax", %progbits
.global portuguese_m22_dump
.type portuguese_m22_dump, %function

portuguese_m22_dump:
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

    /* File offset 0x00: 32 live bytes from 0x41003C40. */
    mov     r0, sp
    ldr     r1, =SOURCE_410
    mov     r2, #0x20
    mov     r3, #0
    bl      STOCK_F_WRITE

    /* File offset 0x20: 64 live bytes from 0x41700000. */
    mov     r0, sp
    ldr     r1, =SOURCE_417
    mov     r2, #0x40
    mov     r3, #0
    bl      STOCK_F_WRITE

    mov     r0, sp
    bl      STOCK_F_CLOSE

return_to_caller:
    add     sp, sp, #FRAME_SIZE
    pop     {r4, pc}

.align 2
output_path:
    .asciz  "/DPOSMEM.BIN"

.size portuguese_m22_dump, . - portuguese_m22_dump
.ltorg
