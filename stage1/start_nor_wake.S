.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function

.extern nor_wake_main

.equ SAVED_CONTEXT, 0x00007F00
.equ TEST_STACK_TOP, 0x00009F00

_start:
    /*
     * Preserve the context supplied by FEL/xfel exec.
     */
    ldr     r0, =SAVED_CONTEXT

    str     sp, [r0, #0]
    str     lr, [r0, #4]

    mrs     r1, cpsr
    str     r1, [r0, #8]

    /*
     * Use a private SRAM stack while executing the test.
     */
    ldr     sp, =TEST_STACK_TOP

    cpsid   if

    bl      nor_wake_main

    /*
     * Preserve the return value only for debugging if needed.
     * Then restore the original FEL execution context.
     */
    mov     r4, r0

    ldr     r0, =SAVED_CONTEXT

    ldr     sp, [r0, #0]
    ldr     lr, [r0, #4]
    ldr     r1, [r0, #8]

    msr     cpsr_cxsf, r1

    dsb     sy
    isb     sy

    /*
     * Return to the caller established by xfel exec.
     */
    bx      lr

.size _start, . - _start
