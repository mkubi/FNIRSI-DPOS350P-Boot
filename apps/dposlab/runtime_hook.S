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
.equ STOCK_FPGA_PHASE_A_TARGET, 0x40049094
.equ DPOSLAB_CP_FPGA_PHASE_A,   0x10
.equ FPGA_PHASE_A_CONTINUE,     0x400491AC

.global dposlab_fpga_phase_a_hook
.type dposlab_fpga_phase_a_hook, %function

.extern dposlab_capture_checkpoint

dposlab_fpga_phase_a_hook:
    /*
     * Replaces:
     *
     *   0x400491A8: BL 0x40049094
     *
     * LR on entry is therefore 0x400491AC.
     */

    /* Preserve the exact pre-call stock context. */
    push    {r0-r12, lr}

    /* Restore original arguments while keeping the saved frame. */
    ldmia   sp, {r0-r12}

    /* Execute the original stock function. */
    ldr     r12, =STOCK_FPGA_PHASE_A_TARGET
    blx     r12

    /*
     * Capture the exact post-call state before invoking C.
     * This second frame is passed to dposlab_capture_checkpoint().
     */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_FPGA_PHASE_A
    ldr     r1, =FPGA_PHASE_A_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /*
     * Restore the state returned by the original stock function.
     */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /*
     * Recover the original continuation LR from the pre-call frame,
     * discard that frame, and resume at 0x400491AC.
     */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_fpga_phase_a_hook, . - dposlab_fpga_phase_a_hook

.equ STOCK_FPGA_EARLY_TARGET,    0x400660C8
.equ DPOSLAB_CP_FPGA_EARLY_INIT, 0x0F
.equ FPGA_EARLY_CONTINUE,        0x40049198

.global dposlab_fpga_early_hook
.type dposlab_fpga_early_hook, %function

.extern dposlab_capture_checkpoint

dposlab_fpga_early_hook:
    /*
     * Replaces:
     *
     *     0x40049194: BL 0x400660C8
     *
     * LR on entry is therefore 0x40049198.
     */

    /*
     * Preserve the original stock call-site context.
     * Fourteen registers = 56 bytes.
     */
    push    {r0-r12, lr}

    /*
     * Restore the original input registers without discarding the saved
     * frame, then call the original early-init function.
     */
    ldmia   sp, {r0-r12}
    ldr     r12, =STOCK_FPGA_EARLY_TARGET
    blx     r12

    /*
     * Capture exact post-call GPR state before invoking diagnostic C code.
     */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_FPGA_EARLY_INIT
    ldr     r1, =FPGA_EARLY_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /*
     * Restore the state returned by the original function.
     */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /*
     * Recover the original continuation LR from the pre-call frame,
     * discard the frame and continue at 0x40049198.
     */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_fpga_early_hook, . - dposlab_fpga_early_hook

.equ STOCK_CSI_CLOCK_RESET_TARGET,     0x40065F48
.equ DPOSLAB_CP_CSI_AFTER_CLOCK_RESET, 0x20
.equ CSI_CLOCK_RESET_CONTINUE,         0x400660D8

.global dposlab_csi_clock_reset_hook
.type dposlab_csi_clock_reset_hook, %function

.extern dposlab_capture_checkpoint

dposlab_csi_clock_reset_hook:
    /*
     * Replaces:
     *
     *     0x400660D4: BL 0x40065F48
     *
     * LR on entry is therefore 0x400660D8.
     */

    /*
     * Preserve original pre-call context.
     * Fourteen registers = 56 bytes.
     */
    push    {r0-r12, lr}

    /*
     * Restore original input registers while retaining the saved frame,
     * then execute the original stock helper.
     */
    ldmia   sp, {r0-r12}
    ldr     r12, =STOCK_CSI_CLOCK_RESET_TARGET
    blx     r12

    /*
     * Save the exact post-call GPR state before invoking diagnostic C.
     */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_CSI_AFTER_CLOCK_RESET
    ldr     r1, =CSI_CLOCK_RESET_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /*
     * Restore the state returned by the original helper.
     */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /*
     * Recover original continuation LR from the pre-call frame.
     */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_csi_clock_reset_hook, . - dposlab_csi_clock_reset_hook

.equ STOCK_CSI_BASE_BIND_TARGET,     0x400658BC
.equ DPOSLAB_CP_CSI_AFTER_BASE_BIND, 0x21
.equ CSI_BASE_BIND_CONTINUE,         0x400660E8

.global dposlab_csi_base_bind_hook
.type dposlab_csi_base_bind_hook, %function

.extern dposlab_capture_checkpoint

dposlab_csi_base_bind_hook:
    /*
     * Replaces 0x400660E4: BL 0x400658BC.
     * LR on entry is 0x400660E8.
     */
    push    {r0-r12, lr}

    /* Execute the original helper with the unmodified input registers. */
    ldmia   sp, {r0-r12}
    ldr     r12, =STOCK_CSI_BASE_BIND_TARGET
    blx     r12

    /* Capture the exact post-call state before entering diagnostic C. */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_CSI_AFTER_BASE_BIND
    ldr     r1, =CSI_BASE_BIND_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /* Restore the state returned by the original stock helper. */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /* Recover the stock continuation and discard the pre-call frame. */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_csi_base_bind_hook, . - dposlab_csi_base_bind_hook

.equ STOCK_CSI_ENABLE_A_TARGET,     0x40065C68
.equ DPOSLAB_CP_CSI_AFTER_ENABLE_A, 0x22
.equ CSI_ENABLE_A_CONTINUE,         0x400660F0

.global dposlab_csi_enable_a_hook
.type dposlab_csi_enable_a_hook, %function

.extern dposlab_capture_checkpoint

dposlab_csi_enable_a_hook:
    /*
     * Replaces 0x400660EC: BL 0x40065C68.
     * LR on entry is 0x400660F0.
     */
    push    {r0-r12, lr}

    /* Execute the original helper with the unmodified input registers. */
    ldmia   sp, {r0-r12}
    ldr     r12, =STOCK_CSI_ENABLE_A_TARGET
    blx     r12

    /* Capture the exact post-call state before entering diagnostic C. */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_CSI_AFTER_ENABLE_A
    ldr     r1, =CSI_ENABLE_A_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /* Restore the state returned by the original stock helper. */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /* Recover the stock continuation and discard the pre-call frame. */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_csi_enable_a_hook, . - dposlab_csi_enable_a_hook

.equ STOCK_CSI_ENABLE_B_TARGET,     0x40065C64
.equ DPOSLAB_CP_CSI_AFTER_ENABLE_B, 0x23
.equ CSI_ENABLE_B_CONTINUE,         0x400660F8

.global dposlab_csi_enable_b_hook
.type dposlab_csi_enable_b_hook, %function

.extern dposlab_capture_checkpoint

dposlab_csi_enable_b_hook:
    /*
     * Replaces 0x400660F4: BL 0x40065C64.
     * LR on entry is 0x400660F8.
     */
    push    {r0-r12, lr}

    /* Execute the original helper with the unmodified input registers. */
    ldmia   sp, {r0-r12}
    ldr     r12, =STOCK_CSI_ENABLE_B_TARGET
    blx     r12

    /* Capture the exact post-call state before entering diagnostic C. */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_CSI_AFTER_ENABLE_B
    ldr     r1, =CSI_ENABLE_B_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /* Restore the state returned by the original stock helper. */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /* Recover the stock continuation and discard the pre-call frame. */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_csi_enable_b_hook, . - dposlab_csi_enable_b_hook

.equ STOCK_CSI_CONFIG_8000_TARGET,     0x40065FA8
.equ DPOSLAB_CP_CSI_AFTER_CONFIG_8000, 0x24
.equ CSI_CONFIG_8000_CONTINUE,         0x40066100

.global dposlab_csi_config_8000_hook
.type dposlab_csi_config_8000_hook, %function

.extern dposlab_capture_checkpoint

dposlab_csi_config_8000_hook:
    /*
     * Replaces 0x400660FC: BL 0x40065FA8.
     * LR on entry is 0x40066100.
     */
    push    {r0-r12, lr}

    /* Execute the original helper with r0 = 8000 and stock context. */
    ldmia   sp, {r0-r12}
    ldr     r12, =STOCK_CSI_CONFIG_8000_TARGET
    blx     r12

    /* Capture the exact post-call state before entering diagnostic C. */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_CSI_AFTER_CONFIG_8000
    ldr     r1, =CSI_CONFIG_8000_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /* Restore the state returned by the original stock helper. */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /* Recover the stock continuation and discard the pre-call frame. */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_csi_config_8000_hook, . - dposlab_csi_config_8000_hook

.equ STOCK_CSI_PINMUX_TARGET,     0x40065EF8
.equ DPOSLAB_CP_CSI_AFTER_PINMUX, 0x25
.equ CSI_PINMUX_CONTINUE,         0x40066064

.global dposlab_csi_pinmux_hook
.type dposlab_csi_pinmux_hook, %function

.extern dposlab_capture_checkpoint

dposlab_csi_pinmux_hook:
    /*
     * Replaces 0x40066060: BL 0x40065EF8.
     * LR on entry is 0x40066064.
     */
    push    {r0-r12, lr}

    /* Execute the original helper with the unmodified stock context. */
    ldmia   sp, {r0-r12}
    ldr     r12, =STOCK_CSI_PINMUX_TARGET
    blx     r12

    /* Capture the exact post-call state before entering diagnostic C. */
    push    {r0-r12, lr}

    mov     r0, #DPOSLAB_CP_CSI_AFTER_PINMUX
    ldr     r1, =CSI_PINMUX_CONTINUE
    mrs     r2, cpsr
    mov     r3, sp
    bl      dposlab_capture_checkpoint

    /* Restore the state returned by the original stock helper. */
    ldmia   sp, {r0-r12}
    add     sp, sp, #56

    /* Recover the stock continuation and discard the pre-call frame. */
    ldr     lr, [sp, #52]
    add     sp, sp, #56
    bx      lr

.size dposlab_csi_pinmux_hook, . - dposlab_csi_pinmux_hook
