.syntax unified
.cpu cortex-a7
.arm
.equ DPOSDBG_PATCH_ARM_B,  1
.equ DPOSDBG_PATCH_ARM_BL, 2
.macro HOOK site, expected, veneer, target, type
    .word \site
    .word \expected
    .word \veneer
    .word \target
    .word \type
    .word 0
.endm
.section .api, "a", %progbits
.global dposdbg_header
.type dposdbg_header, %object
.extern __image_size
.extern dposdbg_trace
.extern dposdbg_screenshot_hook
.extern dposdbg_fpga_selector_wrapper
.extern dposdbg_fpga_write_wrapper
.extern dposdbg_fpga_read_wrapper
.extern dposdbg_smec_init_wrapper
dposdbg_header:
    .word 0x30474244
    .word 2
    .word __image_size
    .word dposdbg_trace
    .word 53
    .word dposdbg_hook_table - dposdbg_header
    .word 0
    .word 0
.size dposdbg_header, . - dposdbg_header

.global dposdbg_hook_table
dposdbg_hook_table:
    HOOK 0x400350F0, 0xEAFFFC63, 0x40055AD4, dposdbg_screenshot_hook, DPOSDBG_PATCH_ARM_B
    HOOK 0x40049E1C, 0xEBFFFD16, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049E20, 0xEBFFFCE4, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049F6C, 0xEBFFFCC2, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049F88, 0xEBFFFCBB, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049F90, 0xEBFFFC9F, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049F9C, 0xEBFFFC9C, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049FA8, 0xEBFFFC99, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049FC4, 0xEBFFFCAC, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049FCC, 0xEBFFFC90, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049FE8, 0xEBFFFCA3, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049FF0, 0xEBFFFC87, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049FF8, 0xEBFFFC9F, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049FFC, 0xEBFFFC6D, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A00C, 0xEBFFFC9A, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A02C, 0xEBFFFC92, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A030, 0xEBFFFC60, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A04C, 0xEBFFFC8A, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A050, 0xEBFFFC58, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A070, 0xEBFFFC81, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A088, 0xEBFFFC7B, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A08C, 0xEBFFFC49, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A094, 0xEBFFFC47, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A09C, 0xEBFFFC45, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004A0A4, 0xEBFFFC43, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049F78, 0xEAFFFCA5, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x40049FB4, 0xEAFFFC96, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x40049FD8, 0xEAFFFC8D, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004A01C, 0xEAFFFC7C, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004A07C, 0xEAFFFC4D, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x40034750, 0xEB005681, 0x40055AF4, dposdbg_smec_init_wrapper, DPOSDBG_PATCH_ARM_BL

    /* Frontend direct FPGA coverage: selectors 0x28..0x30 and 0x62. */
    HOOK 0x4004AA4C, 0xEBFFFA0A, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AA54, 0xEBFFF9EE, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AA7C, 0xEBFFF9FE, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AA90, 0xEAFFF9DF, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004AA9C, 0xEBFFF9F6, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AAAC, 0xEBFFF9D8, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AABC, 0xEAFFF9D4, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004AAE8, 0xEBFFF9E3, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AAFC, 0xEBFFF9C4, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AB6C, 0xEBFFF9C2, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AB80, 0xEAFFF9A3, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004AB90, 0xEBFFF9B9, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004ABA4, 0xEAFFF99A, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004ABB4, 0xEBFFF9B0, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004ABC8, 0xEAFFF991, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004ABDC, 0xEBFFF9A6, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004ABEC, 0xEBFFF988, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AC00, 0xEAFFF983, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004AC2C, 0xEBFFF992, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004AC40, 0xEBFFF973, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004ACB0, 0xEBFFF971, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004ACC4, 0xEAFFF952, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B

