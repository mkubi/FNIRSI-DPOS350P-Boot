.syntax unified
.cpu cortex-a7
.arm

.equ STOCK_FPGA_START, 0x40049188

.section .text.runtime_hook, "ax", %progbits

.global dposlab_fpga_start_hook
.type dposlab_fpga_start_hook, %function

.extern dposlab_fpga_return_monitor

dposlab_fpga_start_hook:
    /*
     * Entry:
     *   LR = stock continuation 0x4003473C
     *
     * Keep the original stock caller context intact while DPOSLAB
     * executes FPGA_Start and its monitor.
     *
     * Fourteen registers = 56 bytes, preserving 8-byte alignment.
     */
    push    {r0-r12, lr}

    ldr     r12, =STOCK_FPGA_START
    blx     r12

    /*
     * Capture the post-FPGA CPSR before entering C.
     * r0 is the first monitor argument.
     */
    mrs     r0, cpsr
    bl      dposlab_fpga_return_monitor

    /*
     * Restore the context present at the patched stock call-site.
     * Popping saved LR into PC resumes at 0x4003473C.
     */
    pop     {r0-r12, pc}

.size dposlab_fpga_start_hook, . - dposlab_fpga_start_hook
