.syntax unified
.cpu cortex-a7
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function
_start:
    /* Required by the current Stage 2 DPOSAPP.BIN loader signature check. */
    ldr     sp, [pc, #0x8c]
    b       app_entry

    /* Place the stack literal exactly at entry + 0x94. */
    .space  0x8c, 0
    .word   0x42D00000

app_entry:
    cpsid   if
    dsb     sy
    isb     sy
    bl      main
1:
    wfi
    b       1b
.size _start, . - _start
