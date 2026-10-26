.syntax unified
.cpu cortex-a7
.arm

.section .text.stock_jump, "ax", %progbits
.global stock_jump_exe2_with_sp
.type stock_jump_exe2_with_sp, %function

stock_jump_exe2_with_sp:
    /* r0 = stock entry, r1 = captured Stage 2 SP. No return is expected. */
    cpsid   if
    mov     sp, r1
    dsb     sy
    isb     sy
    blx     r0

1:
    wfi
    b       1b

.size stock_jump_exe2_with_sp, . - stock_jump_exe2_with_sp
