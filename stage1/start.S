.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function

.equ FEL_CONTEXT_ADDR, 0x00000040
.equ STAGE1_RESULT_ADDR, 0x00007F00
.equ STAGE1_MAGIC,       0x31475453
.equ STAGE1_DONE,        0x00000003

_start:
    /* Match the cache-maintenance preamble used by the XFEL V3s/S3 payload. */
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 6
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 4

    /* Save the FEL caller context exactly in the low SRAM context block. */
    ldr     r0, =FEL_CONTEXT_ADDR
    str     sp, [r0, #0]
    str     lr, [r0, #4]

    mrs     r1, cpsr
    str     r1, [r0, #8]

    mrc     p15, 0, r1, c1, c0, 0
    str     r1, [r0, #12]

    mrc     p15, 0, r1, c12, c0, 0
    str     r1, [r0, #16]

    mrc     p15, 0, r1, c1, c0, 0
    str     r1, [r0, #20]

    /*
     * Publish a compact result block at scratchpad + 0x100.
     * No C code, BSS, private stack or DRAM access is used in this checkpoint.
     */
    ldr     r0, =STAGE1_RESULT_ADDR
    ldr     r1, =STAGE1_MAGIC
    str     r1, [r0, #0]

    ldr     r1, =STAGE1_DONE
    str     r1, [r0, #4]

    ldr     r1, =_start
    str     r1, [r0, #8]

    ldr     r1, =0xA55A5AA5
    str     r1, [r0, #12]

    ldr     r1, =0x5AA5A55A
    str     r1, [r0, #16]

    /* Store the original FEL SP and LR as additional diagnostics. */
    ldr     r2, =FEL_CONTEXT_ADDR
    ldr     r1, [r2, #0]
    str     r1, [r0, #20]
    ldr     r1, [r2, #4]
    str     r1, [r0, #24]

    dsb     sy
    isb     sy

    /* Restore the same system context restored by the XFEL DDR payload. */
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
