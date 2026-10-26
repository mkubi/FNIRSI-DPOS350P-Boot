.syntax unified
.cpu cortex-a7
.arm
.section .text.start,"ax",%progbits
.global _start
_start:
    /* Stage 2 passes dposlab_handoff_t * in r0. */
    mov r4, r0
    ldr sp, =0x42E80000
    cpsid if
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:  cmp r0, r1
    strlo r2, [r0], #4
    blo 1b
    mov r0, r4
    bl main
2:  wfi
    b 2b
