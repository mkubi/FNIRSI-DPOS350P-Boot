.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function

_start:
    cpsid   if

    ldr     sp, =0x0000A000

    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0

1:
    cmp     r0, r1
    bcs     2f
    str     r2, [r0], #4
    b       1b

2:
    dsb     sy
    isb     sy

    bl      main

3:
    wfi
    b       3b

.size _start, . - _start
