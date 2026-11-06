.syntax unified
.cpu cortex-a7
.arm
.text
.align 2

.global fat_diag_call_exe1_usb
.type fat_diag_call_exe1_usb, %function
fat_diag_call_exe1_usb:
    /* Keep IRQ/FIQ masked while the stock USB stack installs its handlers. */
    cpsid   if

    mov     r0, #0
    mov     r1, #0
    mov     r2, #0
    mov     r3, #0
    ldr     r12, =0x4001CE58
    blx     r12

    /* USB setup returned. IRQs must run the stock MSC engine. */
    cpsie   i
1:
    wfi
    b       1b
.size fat_diag_call_exe1_usb, .-fat_diag_call_exe1_usb
