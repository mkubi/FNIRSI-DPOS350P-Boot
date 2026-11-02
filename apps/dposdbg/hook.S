.syntax unified
.cpu cortex-a7
.arm

.equ STOCK_POST_SAVE_CONTINUE, 0x40034284

.section .text.hook, "ax", %progbits
.global dposdbg_screenshot_hook
.extern dposdbg_after_screenshot
.type dposdbg_screenshot_hook, %function

dposdbg_screenshot_hook:
    /*
     * Transparent post-save trampoline.
     *
     * Original instruction replaced at 0x400350F0:
     *     B 0x40034284
     *
     * The stock epilogue POP at 0x400350EC has already restored SP,
     * r4-r6 and LR. Preserve caller-clobbered state around the C hook,
     * keep the stack 8-byte aligned, then tail-branch to the exact stock
     * continuation.
     */
    push    {r0-r3, r12, lr}
    bl      dposdbg_after_screenshot
    pop     {r0-r3, r12, lr}
    b       STOCK_POST_SAVE_CONTINUE

.size dposdbg_screenshot_hook, . - dposdbg_screenshot_hook


.equ STOCK_SMEC_INIT, 0x4004A15C

.global dposdbg_smec_init_wrapper
.extern dposdbg_capture_smec_init
.extern dposdbg_smec_trace_begin
.extern dposdbg_smec_trace_end
.extern dposdbg_capture_checkpoint
.type dposdbg_smec_init_wrapper, %function
dposdbg_smec_init_wrapper:
    /* Preserve the stock caller state and return value around the snapshot. */
    push    {r0-r3, r12, lr}
    bl      dposdbg_smec_trace_begin
    ldr     r12, =STOCK_SMEC_INIT
    blx     r12
    str     r0, [sp]
    bl      dposdbg_smec_trace_end
    bl      dposdbg_capture_smec_init
    pop     {r0-r3, r12, lr}
    bx      lr
.size dposdbg_smec_init_wrapper, . - dposdbg_smec_init_wrapper


.equ STOCK_FPGA_START,       0x40049188
.equ STOCK_POWER_INIT,       0x4004A198
.equ STOCK_HORIZONTAL_INIT,  0x4003A62C
.equ STOCK_VERTICAL_INIT,    0x4004A50C

.macro INIT_CHECKPOINT_WRAPPER name, stock, index
.global \name
.type \name, %function
\name:
    push    {r0-r3, r12, lr}
    ldr     r12, =\stock
    blx     r12
    str     r0, [sp]
    mov     r0, #\index
    bl      dposdbg_capture_checkpoint
    pop     {r0-r3, r12, lr}
    bx      lr
.size \name, . - \name
.endm

INIT_CHECKPOINT_WRAPPER dposdbg_fpga_start_wrapper,      STOCK_FPGA_START,      0
INIT_CHECKPOINT_WRAPPER dposdbg_power_init_wrapper,      STOCK_POWER_INIT,      2
INIT_CHECKPOINT_WRAPPER dposdbg_horizontal_init_wrapper, STOCK_HORIZONTAL_INIT, 3
INIT_CHECKPOINT_WRAPPER dposdbg_vertical_init_wrapper,   STOCK_VERTICAL_INIT,   4


.global dposdbg_late_init_dispatcher
.type dposdbg_late_init_dispatcher, %function
dposdbg_late_init_dispatcher:
    /* The shared veneer preserves LR from the patched stock call site. */
    push    {r0-r7, r12, lr}
    mov     r4, lr

    ldr     r5, =0x40034764
    cmp     r4, r5
    ldreq   r5, =0x4003EF1C
    moveq   r6, #5
    beq     1f
    ldr     r5, =0x40034768
    cmp     r4, r5
    ldreq   r5, =0x4004D164
    moveq   r6, #6
    beq     1f
    ldr     r5, =0x4003476C
    cmp     r4, r5
    ldreq   r5, =0x4004F134
    moveq   r6, #7
    beq     1f
    ldr     r5, =0x40034770
    cmp     r4, r5
    ldreq   r5, =0x40051CF0
    moveq   r6, #8
    beq     1f
    ldr     r5, =0x40034774
    cmp     r4, r5
    ldreq   r5, =0x4003E708
    moveq   r6, #9
    beq     1f
    ldr     r5, =0x40034778
    cmp     r4, r5
    ldreq   r5, =0x4003D6AC
    moveq   r6, #10
    beq     1f
    ldr     r5, =0x4003477C
    cmp     r4, r5
    ldreq   r5, =0x40049A9C
    moveq   r6, #11
    beq     1f
    ldr     r5, =0x40034780
    cmp     r4, r5
    ldreq   r5, =0x40052098
    moveq   r6, #12
    beq     1f
    ldr     r5, =0x40034784
    cmp     r4, r5
    ldreq   r5, =0x4003CF58
    moveq   r6, #13
    beq     1f

    /* Fail closed if the dispatcher is reached from an unknown call site. */
    pop     {r0-r7, r12, lr}
    bx      lr

1:
    /* Restore original call arguments before invoking the stock function. */
    ldmia   sp, {r0-r3}
    blx     r5
    str     r0, [sp]
    mov     r0, r6
    bl      dposdbg_capture_checkpoint
    pop     {r0-r7, r12, lr}
    bx      lr
.size dposdbg_late_init_dispatcher, . - dposdbg_late_init_dispatcher


.equ STOCK_CSI_DESTINATION, 0x40066030
.equ STOCK_CSI_ACTIVE,      0x400660AC
.equ STOCK_CSI_DONE,        0x4003FA04
.extern dposdbg_capture_csi_phase

.macro CSI_POST_WRAPPER name, stock, phase
.global \name
.type \name, %function
\name:
    push    {r0-r3, r12, lr}
    ldr     r12, =\stock
    blx     r12
    str     r0, [sp]
    mov     r0, #\phase
    bl      dposdbg_capture_csi_phase
    pop     {r0-r3, r12, lr}
    bx      lr
.size \name, . - \name
.endm

CSI_POST_WRAPPER dposdbg_csi_destination_wrapper, STOCK_CSI_DESTINATION, 0
CSI_POST_WRAPPER dposdbg_csi_active_wrapper,      STOCK_CSI_ACTIVE,      1
CSI_POST_WRAPPER dposdbg_csi_done_wrapper,        STOCK_CSI_DONE,        2


.equ STOCK_SC3_PARAMETER_JOB_HELPER, 0x40049EC4
.global dposdbg_sc3_job_args_wrapper
.extern dposdbg_capture_sc3_job_args
.type dposdbg_sc3_job_args_wrapper, %function
dposdbg_sc3_job_args_wrapper:
    push    {r0-r3, r12, lr}
    bl      dposdbg_capture_sc3_job_args
    pop     {r0-r3, r12, lr}
    ldr     r12, =STOCK_SC3_PARAMETER_JOB_HELPER
    bx      r12
.size dposdbg_sc3_job_args_wrapper, . - dposdbg_sc3_job_args_wrapper

.equ STOCK_ACQ_WINDOW_APPLY, 0x4003FAB0

.global dposdbg_acq_window_wrapper
.extern dposdbg_capture_acq_window
.type dposdbg_acq_window_wrapper, %function
dposdbg_acq_window_wrapper:
    /* Replaces 0x4003FB80: BL 0x4003FAB0. */
    push    {r0-r3, r12, lr}
    bl      dposdbg_capture_acq_window
    pop     {r0-r3, r12, lr}

    /* Preserve the stock LR and continue through the original trampoline. */
    ldr     r12, =STOCK_ACQ_WINDOW_APPLY
    bx      r12
.size dposdbg_acq_window_wrapper, . - dposdbg_acq_window_wrapper

.equ STOCK_ACQ_POSITION_FOLLOWUP, 0x4003FB90

.global dposdbg_acq_position_wrapper
.extern dposdbg_capture_acq_position
.type dposdbg_acq_position_wrapper, %function
dposdbg_acq_position_wrapper:
    /* Replaces 0x40041540: BL 0x4003FB90. */
    push    {r0-r3, r12, lr}
    bl      dposdbg_capture_acq_position
    pop     {r0-r3, r12, lr}

    /* Original LR still points to 0x40041544. */
    ldr     r12, =STOCK_ACQ_POSITION_FOLLOWUP
    bx      r12
.size dposdbg_acq_position_wrapper, . - dposdbg_acq_position_wrapper

.equ STOCK_SAMPLE_POINTER_PREPARE, 0x40040030

.global dposdbg_acq_followup_wrapper
.extern dposdbg_capture_acq_followup
.type dposdbg_acq_followup_wrapper, %function
dposdbg_acq_followup_wrapper:
    /* Replaces 0x40041574: BL 0x40040030. */
    push    {r0-r3, r12, lr}
    bl      dposdbg_capture_acq_followup
    pop     {r0-r3, r12, lr}

    /* Original LR still points to 0x40041578. */
    ldr     r12, =STOCK_SAMPLE_POINTER_PREPARE
    bx      r12
.size dposdbg_acq_followup_wrapper, . - dposdbg_acq_followup_wrapper

.equ STOCK_STREAM_CONFIGURE, 0x400405E0

.global dposdbg_stream_config_wrapper
.extern dposdbg_capture_stream_config
.type dposdbg_stream_config_wrapper, %function
dposdbg_stream_config_wrapper:
    /* Replaces 0x40041584: BL 0x400405E0. */
    push    {r0-r3, r12, lr}
    bl      dposdbg_capture_stream_config
    pop     {r0-r3, r12, lr}

    /* Original LR still points to 0x40041588. */
    ldr     r12, =STOCK_STREAM_CONFIGURE
    bx      r12
.size dposdbg_stream_config_wrapper, . - dposdbg_stream_config_wrapper

.equ STOCK_UDIV64_HELPER, 0x40034590

.global dposdbg_horizontal_source_wrapper
.extern dposdbg_capture_horizontal_source
.type dposdbg_horizontal_source_wrapper, %function
dposdbg_horizontal_source_wrapper:
    /* Replaces 0x4003B1C0: BL 0x40034590. */
    push    {r4, r12, lr}
    ldr     r12, =STOCK_UDIV64_HELPER
    blx     r12

    /* Preserve the stock 64-bit quotient across the diagnostic callback. */
    mov     r4, r0
    mov     r12, r1
    push    {r0-r3, r12}
    mov     r0, r4
    mov     r1, r12
    bl      dposdbg_capture_horizontal_source
    pop     {r0-r3, r12}
    mov     r0, r4
    mov     r1, r12
    pop     {r4, r12, pc}
.size dposdbg_horizontal_source_wrapper, . - dposdbg_horizontal_source_wrapper

.equ STOCK_HORIZONTAL_WRITER1_CONTINUE, 0x4003B1C8

.equ STOCK_APPLY_HORIZONTAL, 0x4003A654

.global dposdbg_sysconfig_horizontal_wrapper
.extern dposdbg_sysconfig_trace_begin
.extern dposdbg_sysconfig_horizontal_done
.type dposdbg_sysconfig_horizontal_wrapper, %function
dposdbg_sysconfig_horizontal_wrapper:
    /* Replaces SysConfigExec call at 0x4003E724. */
    push    {r0-r3, r12, lr}
    bl      dposdbg_sysconfig_trace_begin
    ldmia   sp, {r0-r3}
    ldr     r12, =STOCK_APPLY_HORIZONTAL
    blx     r12
    str     r0, [sp]
    bl      dposdbg_sysconfig_horizontal_done
    pop     {r0-r3, r12, pc}
.size dposdbg_sysconfig_horizontal_wrapper, . - dposdbg_sysconfig_horizontal_wrapper

