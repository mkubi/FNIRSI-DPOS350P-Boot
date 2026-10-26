.syntax unified
.cpu cortex-a7
.arm

.section .text.dposlab_jump, "ax", %progbits
.global dposlab_jump_stage3
.type dposlab_jump_stage3, %function

dposlab_jump_stage3:
    /* Input: r0 = Stage 3 entry, r1 = dposlab_handoff_t pointer. */
    cpsid   if
    mov     r2, r0
    mov     r0, r1
    dsb     sy
    isb     sy
    bx      r2

1:
    wfi
    b       1b

.size dposlab_jump_stage3, . - dposlab_jump_stage3
