.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function

.extern sys_clock_init
.extern sys_dram_init
.extern stage1_stg2_report

.equ FEL_CONTEXT_ADDR, 0x00000040

_start:
    /*
     * Synchronize the execution pipeline before changing clocks
     * and initializing DRAM.
     */
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 6
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 4

    /*
     * Preserve the original FEL execution context.
     *
     * This context is restored only when stage1_stg2_report()
     * returns, for example after a Stage 2 validation failure.
     */
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
     * Native hardware initialization.
     */
    bl      sys_clock_init
    bl      sys_dram_init

    /*
     * Pass the original FEL SP and LR to the Stage 1 diagnostic
     * and Stage 2 loading routine.
     */
    ldr     r2, =FEL_CONTEXT_ADDR
    ldr     r0, [r2, #0]
    ldr     r1, [r2, #4]
    bl      stage1_stg2_report

    /*
     * This path is reached only when Stage 2 was not started.
     * Restore the original FEL execution context and return.
     */
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


.align 4
.global stage2_handoff
.type stage2_handoff, %function

stage2_handoff:
    /*
     * Input:
     *   r0 = validated Stage 2 entry address
     *
     * The Stage 2 payload has already been loaded into DRAM and
     * its CRC32 has been verified by Stage 1.
     */

    /*
     * Stage 2 startup is entered with IRQ and FIQ disabled.
     */
    cpsid   if

    /*
     * Ensure that all Stage 1 writes to the Stage 2 DRAM region
     * have completed before instruction execution begins.
     */
    dsb     sy

    /*
     * Stage 2 was loaded through data writes. Invalidate the
     * instruction cache and branch predictor before executing
     * instructions from the newly populated DRAM region.
     */
    mov     r1, #0
    mcr     p15, 0, r1, c7, c5, 0
    mcr     p15, 0, r1, c7, c5, 6

    dsb     sy
    isb     sy

    /*
     * Keep r0 unchanged because it contains the Stage 2 entry
     * address. Clear the remaining argument and scratch registers.
     */
    mov     r1, #0
    mov     r2, #0
    mov     r3, #0
    mov     r12, #0
    mov     lr, #0

    /*
     * Transfer control to Stage 2.
     */
    bx      r0

    /*
     * Stage 2 must not return. This loop is only a defensive
     * fallback in case control somehow continues past bx r0.
     */
1:
    b       1b

.size stage2_handoff, . - stage2_handoff
