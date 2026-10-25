.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function

_start:
    ldr     sp, [pc, #0x8c]
    b       1f

    .space  0x8c, 0

    .word   0x41020000

1:
    cpsid   if

    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0

2:
    cmp     r0, r1
    bcs     3f
    str     r2, [r0], #4
    b       2b

3:
    dsb     sy
    isb     sy

    bl      main

4:
    wfi
    b       4b

.size _start, . - _start
