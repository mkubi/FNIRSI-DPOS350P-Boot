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
