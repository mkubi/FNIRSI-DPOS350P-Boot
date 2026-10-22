.syntax unified
.cpu cortex-a7
.arm

.section .text.bt0_header, "ax", %progbits
.global _start
.type _start, %function

.extern sys_clock_init
.extern sys_dram_init
.extern stage1_stg2_report

.equ BT0_STACK_TOP, 0x00007C00

_start:
    /* 32-byte DPOS350P/V3s compact eGON.BT0 header. */
    b       bt0_entry
    .ascii  "eGON.BT0"
    .word   0x5F0A6C39          /* checksum stamp, patched by mkbt0.py */
    .word   0x00006800          /* fixed original-compatible image length */
    .byte   'S', 'P', 'L', 2
    .word   0
    .word   0

bt0_entry:
    /* BootROM startup must not depend on a FEL-provided stack. */
    cpsid   if
    ldr     sp, =BT0_STACK_TOP

    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 6
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 4
    dsb     sy
    isb     sy

    bl      sys_clock_init
    bl      sys_dram_init

    /* No FEL context exists in autonomous BT0 mode. */
    mov     r0, #0
    mov     r1, #0
    bl      stage1_stg2_report

    /* A validation/load failure returns here. Stay recoverable via FEL. */
1:
    cpsid   if
    wfi
    b       1b

.size _start, . - _start

.align 4
.global stage2_handoff
.type stage2_handoff, %function

stage2_handoff:
    /* r0 = validated Stage 2 entry address. */
    cpsid   if
    dsb     sy

    mov     r1, #0
    mcr     p15, 0, r1, c7, c5, 0
    mcr     p15, 0, r1, c7, c5, 6

    dsb     sy
    isb     sy

    mov     r1, #0
    mov     r2, #0
    mov     r3, #0
    mov     r12, #0
    mov     lr, #0
    bx      r0

2:
    b       2b

.size stage2_handoff, . - stage2_handoff
