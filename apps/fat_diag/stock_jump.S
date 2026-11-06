.syntax unified
.cpu cortex-a7
.arm
.text
.align 2
.global fat_diag_jump_exe1_with_sp
.type fat_diag_jump_exe1_with_sp, %function
fat_diag_jump_exe1_with_sp:
    mov sp, r1
    bx r0
.size fat_diag_jump_exe1_with_sp, .-fat_diag_jump_exe1_with_sp
