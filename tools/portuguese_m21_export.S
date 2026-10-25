.syntax unified
.cpu cortex-a7
.arm

.equ EVENT_STRUCTURE,    0x42E61FF8
.equ SNAPSHOT_ADDRESS,   0x41700000
.equ STOCK_EVENT_UPDATE, 0x4002FD80
.equ STOCK_F_OPEN,       0x40059B8C
.equ STOCK_F_WRITE,      0x4005A1D4
.equ STOCK_F_CLOSE,      0x4005A694

.equ SNAP_MAGIC,         0x3132444D  /* "MD21" little-endian */
.equ SNAP_SIZE,          0x10
.equ FRAME_SIZE,         0x254

.section .text, "ax", %progbits
.global portuguese_m21_export
.type portuguese_m21_export, %function

portuguese_m21_export:
    push    {r4-r7, lr}
    sub     sp, sp, #FRAME_SIZE

    bl      STOCK_EVENT_UPDATE
    ldr     r4, =EVENT_STRUCTURE
    ldrb    r0, [r4, #0x174]
    cmp     r0, #2
    bne     return_to_caller

    mov     r0, #0
    strb    r0, [r4, #0x174]

    ldr     r5, =SNAPSHOT_ADDRESS
    ldr     r0, =SNAP_MAGIC
    str     r0, [r5, #0x00]

    /* TTBCR.N == 0 was established by M2. */
    mrc     p15, 0, r6, c2, c0, 0
    ldr     r7, =0xFFFFC000
    and     r6, r6, r7
    add     r7, r6, #0x1000

    ldr     r0, [r7]
    str     r0, [r5, #0x04]
    ldr     r0, [r7, #0x40]
    str     r0, [r5, #0x08]
    ldr     r0, [r7, #0x5C]
    str     r0, [r5, #0x0C]
    dsb     sy

    mov     r0, sp
    adr     r1, output_path
    mov     r2, #0x0A
    bl      STOCK_F_OPEN
    cmp     r0, #0
    bne     return_to_caller

    mov     r0, sp
    mov     r1, r5
    mov     r2, #SNAP_SIZE
    mov     r3, #0
    bl      STOCK_F_WRITE

    mov     r0, sp
    bl      STOCK_F_CLOSE

return_to_caller:
    add     sp, sp, #FRAME_SIZE
    pop     {r4-r7, pc}

.align 2
output_path:
    .asciz  "/DPOSMMU.BIN"

.size portuguese_m21_export, . - portuguese_m21_export
.ltorg
