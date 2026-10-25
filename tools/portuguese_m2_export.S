.syntax unified
.cpu cortex-a7
.arm

.equ EVENT_STRUCTURE,       0x42E61FF8
.equ SNAPSHOT_ADDRESS,      0x41700000
.equ STOCK_EVENT_UPDATE,    0x4002FD80
.equ STOCK_SCREENSHOT_SAVE, 0x4003506C
.equ STOCK_F_OPEN,          0x40059B8C
.equ STOCK_F_LSEEK,         0x4005A6CC
.equ STOCK_F_WRITE,         0x4005A1D4
.equ STOCK_F_CLOSE,         0x4005A694
.equ SNAP_MAGIC,            0x32554D4D
.equ SNAP_DONE,             0x454E4F44
.equ SNAP_SIZE,             0x1C
.equ FIL_WORK_SIZE,         0x250
.equ FRAME_SIZE,            0x254

.section .text, "ax", %progbits
.global portuguese_m2_export
.type portuguese_m2_export, %function

portuguese_m2_export:
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
    mrc     p15, 0, r0, c1, c0, 0
    str     r0, [r5, #0x04]
    mrc     p15, 0, r0, c2, c0, 0
    str     r0, [r5, #0x08]
    mrc     p15, 0, r0, c2, c0, 1
    str     r0, [r5, #0x0c]
    mrc     p15, 0, r0, c2, c0, 2
    str     r0, [r5, #0x10]
    mrc     p15, 0, r0, c3, c0, 0
    str     r0, [r5, #0x14]
    ldr     r0, =SNAP_DONE
    str     r0, [r5, #0x18]
    dsb     sy

    mov     r0, sp
    adr     r1, output_path
    mov     r2, #0x0A
    bl      STOCK_F_OPEN
    cmp     r0, #0
    bne     export_done

    mov     r0, sp
    mov     r1, r5
    mov     r2, #SNAP_SIZE
    mov     r3, #0
    bl      STOCK_F_WRITE

    mov     r0, sp
    bl      STOCK_F_CLOSE

export_done:
    bl      STOCK_SCREENSHOT_SAVE

return_to_caller:
    add     sp, sp, #FRAME_SIZE
    pop     {r4-r7, pc}

.align 2
output_path:
    .asciz  "/DPOSMMU.BIN"

.size portuguese_m2_export, . - portuguese_m2_export
.ltorg
