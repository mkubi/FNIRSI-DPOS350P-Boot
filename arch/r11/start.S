.syntax unified
.cpu cortex-a7
.arm
.section .text.start, "ax", %progbits
.global _start
.type _start, %function
.extern main
.extern __bss_start
.extern __bss_end
_start:
    ldr sp, =0x41020000
    push {lr}
    mrs r0, cpsr
    orr r0, r0, #0xc0
    msr cpsr_c, r0
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:  cmp r0, r1
    bhs 2f
    str r2, [r0], #4
    b 1b
2:  dsb sy
    isb
    bl main
    dsb sy
    isb
    pop {pc}
.size _start, . - _start
