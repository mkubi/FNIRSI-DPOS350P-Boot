.syntax unified
.cpu cortex-a7
.arm

.equ STOCK_FPGA_START, 0x40049188
.equ CPU_FRAME_SIZE,   56

.section .text.runtime_hook, "ax", %progbits

.global dposlab_fpga_start_hook
.type dposlab_fpga_start_hook, %function

.extern dposlab_capture_before
.extern dposlab_capture_after
.extern dposlab_fpga_return_monitor

dposlab_fpga_start_hook:
    /*
     * Save the context present at the original stock call-site.
     *
     * Layout at SP:
     *     +0x00 r0
     *     ...
     *     +0x30 r12
     *     +0x34 original LR = 0x4003473C
     */
    push    {r0-r12, lr}

    /*
     * BEFORE snapshot.
     *
     * Calling the C recorder may alter caller-saved registers, but the
     * original r0-r12 remain in the saved frame.
     */
    mrs     r0, cpsr
    mov     r1, sp
    bl      dposlab_capture_before

    /*
     * Restore the original argument/register state without removing the
     * saved frame. FPGA_Start therefore executes with the original GPRs,
     * while its SP is the wrapper execution stack below the saved frame.
     */
    ldmia   sp, {r0-r12}

    ldr     r12, =STOCK_FPGA_START
    blx     r12

    /*
     * Capture the exact GPR state returned by FPGA_Start before entering
     * any C code.
     */
    push    {r0-r12, lr}
    mrs     r0, cpsr
    mov     r1, sp
    bl      dposlab_capture_after

    /*
     * Discard the post-call frame. The original pre-call frame remains.
     */
    add     sp, sp, #CPU_FRAME_SIZE

    bl      dposlab_fpga_return_monitor

    /*
     * Restore the original stock context and resume at 0x4003473C.
     */
    pop     {r0-r12, pc}

.size dposlab_fpga_start_hook, . - dposlab_fpga_start_hook

.equ STOCK_POST_SCREENSHOT_CONTINUE, 0x40034284

.global dposlab_screenshot_hook
.type dposlab_screenshot_hook, %function

.extern dposlab_trace_export_after_screenshot

dposlab_screenshot_hook:
    /*
     * Transparent post-save hook.
     *
     * Original instruction replaced at 0x400350F0:
     *
     *     B 0x40034284
     *
     * The stock screenshot epilogue has already restored SP, r4-r6,
     * and LR. Preserve caller-clobbered registers around the exporter,
     * then tail-branch to the original continuation.
     *
     * Six registers = 24 bytes, preserving 8-byte stack alignment.
     */
    push    {r0-r3, r12, lr}
    bl      dposlab_trace_export_after_screenshot
    pop     {r0-r3, r12, lr}
    b       STOCK_POST_SCREENSHOT_CONTINUE

.size dposlab_screenshot_hook, . - dposlab_screenshot_hook
