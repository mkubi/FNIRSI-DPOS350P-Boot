.syntax unified
.cpu cortex-a7
.arm

.equ EVENT_STRUCTURE,         0x42E61FF8
.equ SNAPSHOT_ADDRESS,        0x41700000
.equ STOCK_EVENT_UPDATE,      0x4002FD80
.equ STOCK_SCREENSHOT_SAVE,   0x4003506C

.equ SNAP_MAGIC,              0x30554D4D
.equ SNAP_VERSION,            1
.equ SNAP_SIZE,               0x34
.equ SNAP_DONE,               0x454E4F44

.section .text, "ax", %progbits
.global portuguese_m1_snapshot
.type portuguese_m1_snapshot, %function

portuguese_m1_snapshot:
    /*
     * Replaces the complete language callback at 0x40055AD4.
     *
     * Uses Portuguese as a manual debug trigger:
     *   1. acknowledge Portuguese event
     *   2. capture ARM MMU/fault registers to 0x41700000
     *   3. take one stock screenshot
     *   4. return to the GUI
     *
     * Four registers are pushed so SP remains 8-byte aligned across BL.
     */
    push    {r4-r6, lr}

    /* Preserve the stock event scan/update operation. */
    bl      STOCK_EVENT_UPDATE

    ldr     r4, =EVENT_STRUCTURE

    /* Portuguese accepted event: structure byte +0x174 equals 2. */
    ldrb    r0, [r4, #0x174]
    cmp     r0, #2
    bne     return_to_caller

    /* Acknowledge the event before starting the debug operation. */
    mov     r0, #0
    strb    r0, [r4, #0x174]

    /* Snapshot destination. This region is used only as data RAM. */
    ldr     r5, =SNAPSHOT_ADDRESS

    ldr     r0, =SNAP_MAGIC
    str     r0, [r5, #0x00]

    mov     r0, #SNAP_VERSION
    str     r0, [r5, #0x04]

    mov     r0, #SNAP_SIZE
    str     r0, [r5, #0x08]

    /* System Control Register. */
    mrc     p15, 0, r0, c1, c0, 0
    str     r0, [r5, #0x0C]

    /* Translation Table Base Registers. */
    mrc     p15, 0, r0, c2, c0, 0
    str     r0, [r5, #0x10]

    mrc     p15, 0, r0, c2, c0, 1
    str     r0, [r5, #0x14]

    /* Translation Table Base Control Register. */
    mrc     p15, 0, r0, c2, c0, 2
    str     r0, [r5, #0x18]

    /* Domain Access Control Register. */
    mrc     p15, 0, r0, c3, c0, 0
    str     r0, [r5, #0x1C]

    /* Data Fault Status and Address Registers. */
    mrc     p15, 0, r0, c5, c0, 0
    str     r0, [r5, #0x20]

    mrc     p15, 0, r0, c6, c0, 0
    str     r0, [r5, #0x24]

    /* Instruction Fault Status and Address Registers. */
    mrc     p15, 0, r0, c5, c0, 1
    str     r0, [r5, #0x28]

    mrc     p15, 0, r0, c6, c0, 2
    str     r0, [r5, #0x2C]

    /* Written last so a reader can detect a complete snapshot. */
    ldr     r0, =SNAP_DONE
    str     r0, [r5, #0x30]

    dsb     sy

    /* Visible proof that the snapshot path completed. */
    bl      STOCK_SCREENSHOT_SAVE

return_to_caller:
    pop     {r4-r6, pc}

.size portuguese_m1_snapshot, . - portuguese_m1_snapshot
.ltorg
