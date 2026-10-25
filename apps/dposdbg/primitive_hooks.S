.syntax unified
.cpu cortex-a7
.arm

.equ STOCK_FPGA_READ,     0x400491B8
.equ STOCK_FPGA_WRITE,    0x40049214
.equ STOCK_FPGA_SELECTOR, 0x4004927C
.equ STOCK_CSI_DESTINATION, 0x40066030
.equ SITE_CSI_DESTINATION,  0x40040660
.equ EVENT_CSI_DESTINATION, 0x00002000

.section .text.primitive_hooks, "ax", %progbits
.extern dposdbg_trace_primitive

.global dposdbg_fpga_selector_wrapper
.type dposdbg_fpga_selector_wrapper, %function
dposdbg_fpga_selector_wrapper:
    sub     r12, lr, #4
    ldr     r3, =SITE_CSI_DESTINATION
    cmp     r12, r3
    beq     .Ltrace_csi_destination
    push    {r0-r3, r12, lr}
    mov     r1, r0
    sub     r2, lr, #4
    movw    r0, #0x1000
    bl      dposdbg_trace_primitive
    pop     {r0-r3, r12, lr}

    push    {r0-r3, r12, lr}
    bl      STOCK_FPGA_SELECTOR
    str     r0, [sp]
    pop     {r0-r3, r12, lr}
    bx      lr
.Ltrace_csi_destination:
    push    {r0-r3, r12, lr}
    mov     r1, r0
    sub     r2, lr, #4
    movw    r0, #EVENT_CSI_DESTINATION
    bl      dposdbg_trace_primitive
    pop     {r0-r3, r12, lr}
    b       STOCK_CSI_DESTINATION
.size dposdbg_fpga_selector_wrapper, . - dposdbg_fpga_selector_wrapper

.global dposdbg_fpga_write_wrapper
.type dposdbg_fpga_write_wrapper, %function
dposdbg_fpga_write_wrapper:
    push    {r0-r3, r12, lr}
    mov     r1, r0
    sub     r2, lr, #4
    movw    r0, #0x1001
    bl      dposdbg_trace_primitive
    pop     {r0-r3, r12, lr}

    push    {r0-r3, r12, lr}
    bl      STOCK_FPGA_WRITE
    str     r0, [sp]
    pop     {r0-r3, r12, lr}
    bx      lr
.size dposdbg_fpga_write_wrapper, . - dposdbg_fpga_write_wrapper

.global dposdbg_fpga_read_wrapper
.type dposdbg_fpga_read_wrapper, %function
dposdbg_fpga_read_wrapper:
    push    {r1-r3, r4, r12, lr}
    mov     r4, lr
    bl      STOCK_FPGA_READ

    push    {r0-r3, r12, lr}
    mov     r1, r0
    sub     r2, r4, #4
    movw    r0, #0x1002
    bl      dposdbg_trace_primitive
    pop     {r0-r3, r12, lr}

    pop     {r1-r3, r4, r12, lr}
    bx      lr
.size dposdbg_fpga_read_wrapper, . - dposdbg_fpga_read_wrapper
