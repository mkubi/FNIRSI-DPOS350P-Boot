.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function

.extern stage1_clock_main

.equ FEL_CONTEXT_ADDR, 0x00000040

_start:
    /* Same cache preamble used by the XFEL V3s/S3 DDR payload. */
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 6
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 4

    /* Save the FEL caller context at the same address used by XFEL. */
    ldr     r0, =FEL_CONTEXT_ADDR
    str     sp, [r0, #0]
    str     lr, [r0, #4]

    mrs     lr, cpsr
    str     lr, [r0, #8]

    mrc     p15, 0, lr, c1, c0, 0
    str     lr, [r0, #12]

    mrc     p15, 0, lr, c12, c0, 0
    str     lr, [r0, #16]

    mrc     p15, 0, lr, c1, c0, 0
    str     lr, [r0, #20]

    /*
     * Pass the original FEL SP and LR to C through the normal ARM ABI:
     *
     * r0 = original SP
     * r1 = original LR
     */
    ldr     r2, =FEL_CONTEXT_ADDR
    ldr     r0, [r2, #0]
    ldr     r1, [r2, #4]

    bl      stage1_clock_main

    /* Restore the same context restored by the XFEL DDR payload. */
    ldr     r0, =FEL_CONTEXT_ADDR
    ldr     sp, [r0, #0]
    ldr     lr, [r0, #4]

    ldr     r1, [r0, #20]
    mcr     p15, 0, r1, c1, c0, 0

    ldr     r1, [r0, #16]
    mcr     p15, 0, r1, c12, c0, 0

    ldr     r1, [r0, #12]
    mcr     p15, 0, r1, c1, c0, 0

    ldr     r1, [r0, #8]
    msr     cpsr_cxsf, r1

    dsb     sy
    isb     sy
    bx      lr

.size _start, . - _start
