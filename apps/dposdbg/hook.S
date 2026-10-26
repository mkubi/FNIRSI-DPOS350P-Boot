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
.extern dposdbg_capture_checkpoint
.type dposdbg_smec_init_wrapper, %function
dposdbg_smec_init_wrapper:
    /* Preserve the stock caller state and return value around the snapshot. */
    push    {r0-r3, r12, lr}
    ldr     r12, =STOCK_SMEC_INIT
    blx     r12
    str     r0, [sp]
    bl      dposdbg_capture_smec_init
    mov     r0, #1
    bl      dposdbg_capture_checkpoint
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
