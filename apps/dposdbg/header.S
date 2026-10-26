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
.extern dposdbg_csi_destination_wrapper
.extern dposdbg_csi_active_wrapper
.extern dposdbg_csi_done_wrapper
.extern dposdbg_smec_init_wrapper
.extern dposdbg_fpga_start_wrapper
.extern dposdbg_power_init_wrapper
.extern dposdbg_horizontal_init_wrapper
.extern dposdbg_vertical_init_wrapper
.extern dposdbg_late_init_dispatcher
dposdbg_header:
    .word 0x30474244
    .word 2
    .word __image_size
    .word dposdbg_trace
    .word 48
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
    HOOK 0x4004A148, 0xEBFFFC4B, 0x40055ADC, dposdbg_fpga_selector_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40040660, 0xEB009672, 0x40055B34, dposdbg_csi_destination_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40049F78, 0xEAFFFCA5, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x40049FB4, 0xEAFFFC96, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x40049FD8, 0xEAFFFC8D, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004A01C, 0xEAFFFC7C, 0x40055AE4, dposdbg_fpga_write_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x4004A07C, 0xEAFFFC4D, 0x40055AEC, dposdbg_fpga_read_wrapper, DPOSDBG_PATCH_ARM_B
    HOOK 0x40034750, 0xEB005681, 0x40055AF4, dposdbg_smec_init_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034738, 0xEB005292, 0x40055AFC, dposdbg_fpga_start_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034754, 0xEB00568F, 0x40055B04, dposdbg_power_init_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034758, 0xEB0017B3, 0x40055B0C, dposdbg_horizontal_init_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4003475C, 0xEB00576A, 0x40055B14, dposdbg_vertical_init_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034760, 0xEB0029ED, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034764, 0xEB00627E, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034768, 0xEB006A71, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4003476C, 0xEB00755F, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034770, 0xEB0027E4, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034774, 0xEB0023CC, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034778, 0xEB0054C7, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4003477C, 0xEB007645, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40034780, 0xEB0021F4, 0x40055B1C, dposdbg_late_init_dispatcher, DPOSDBG_PATCH_ARM_BL
    HOOK 0x4004066C, 0xEB00968E, 0x40055B24, dposdbg_csi_active_wrapper, DPOSDBG_PATCH_ARM_BL
    HOOK 0x40040684, 0xEBFFFCDE, 0x40055B2C, dposdbg_csi_done_wrapper, DPOSDBG_PATCH_ARM_BL
