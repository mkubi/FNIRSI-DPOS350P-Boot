.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function

/*
 * The current Custom App loader requires the first word 0xE59FD08C.
 * This is: ldr sp, [pc, #0x8c], whose literal is at image offset 0x94.
 */
_start:
    ldr     sp, [pc, #0x8c]
    b       app_reset

    .space  0x94 - (. - _start)
    .word   0x40FFF000

app_reset:
    cpsid   if

    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
1:
    cmp     r0, r1
    strlo   r2, [r0], #4
    blo     1b

    bl      main

2:
    wfi
    b       2b

.size _start, . - _start
