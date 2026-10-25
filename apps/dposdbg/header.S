.syntax unified
.cpu cortex-a7
.arm

.section .api, "ax", %progbits
.global dposdbg_header
.type dposdbg_header, %object
.extern __image_start
.extern __image_end
.extern __image_size
.extern dposdbg_screenshot_hook
.extern dposdbg_trace
.extern dposdbg_fpga_selector_wrapper
.extern dposdbg_fpga_write_wrapper
.extern dposdbg_fpga_read_wrapper

dposdbg_header:
    .word 0x30474244                 /* "DBG0" */
    .word 1                          /* ABI version */
    .word __image_size
    .word dposdbg_screenshot_hook
    .word dposdbg_trace
    .word dposdbg_fpga_selector_wrapper
    .word dposdbg_fpga_write_wrapper
    .word dposdbg_fpga_read_wrapper
.size dposdbg_header, . - dposdbg_header
