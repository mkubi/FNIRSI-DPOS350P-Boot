.syntax unified
.cpu cortex-a7
.arm

.section .text.loader_jump, "ax", %progbits
.global loader_jump
.type loader_jump, %function

loader_jump:
    /* r0 contains the ARM entry address. */
    cpsid   if
    dsb     sy
    isb     sy
    blx     r0

    /* A boot target is not expected to return. */
1:
    wfi
    b       1b

.size loader_jump, . - loader_jump
