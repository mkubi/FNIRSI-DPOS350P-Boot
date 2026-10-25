.syntax unified
.cpu cortex-a7
.arm

.section .text, "ax", %progbits
.global portuguese_mmu_probe
.type portuguese_mmu_probe, %function

portuguese_mmu_probe:
    /*
     * Replacement for the complete language option handler at 0x40055AD4.
     *
     * Portuguese:
     *   SCTLR.M = 1 -> one screenshot
     *   SCTLR.M = 0 -> two screenshots
     */

    push    {r4, lr}

    /* Preserve the stock event scanning/update step. */
    bl      0x4002FD80

    /* Language-menu event structure. */
    ldr     r4, =0x42E61FF8

    /* Portuguese event is byte +0x174, accepted value is 2. */
    ldrb    r0, [r4, #0x174]
    cmp     r0, #2
    bne     return_to_caller

    /* Acknowledge event before performing the slow screenshot operation. */
    mov     r0, #0
    strb    r0, [r4, #0x174]

    /* Read System Control Register. Bit 0 is SCTLR.M. */
    mrc     p15, 0, r0, c1, c0, 0
    tst     r0, #1
    bne     mmu_enabled

    /* MMU disabled: create the first of two screenshots. */
    bl      0x4003506C

mmu_enabled:
    /* MMU enabled: only this screenshot is created. */
    bl      0x4003506C

return_to_caller:
    pop     {r4, pc}

.size portuguese_mmu_probe, . - portuguese_mmu_probe
.ltorg
