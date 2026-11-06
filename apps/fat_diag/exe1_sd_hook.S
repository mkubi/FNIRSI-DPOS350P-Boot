.syntax unified
.cpu cortex-a7
.arm
.text
.align 2

.global fat_diag_exe1_sd_init_hook
.type fat_diag_exe1_sd_init_hook, %function
fat_diag_exe1_sd_init_hook:
    push    {r4, lr}
    mov     r4, r0
    cmp     r0, #0
    bne     1f

    ldr     r1, =0x400247CC
    ldr     r1, [r1]
    cmp     r1, #1
    bne     2f

    mov     r0, #0
    ldr     r12, =0x40006EF0
    blx     r12
    cmp     r0, #0
    moveq   r0, #0
    movne   r0, #2
    b       2f

1:
    mov     r0, #1

2:
    bl      fat_diag_exe1_sd_init_observer
    b       .
.size fat_diag_exe1_sd_init_hook, .-fat_diag_exe1_sd_init_hook
