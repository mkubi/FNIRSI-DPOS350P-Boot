#include <stdint.h>
#include "../../drivers/fpga_bus_ro.h"
#include "../../drivers/r11_csi.h"
#include "../../arch/r11/cache.h"

#ifndef CSI_DIAG_STAGE
#define CSI_DIAG_STAGE 25
#endif

void diag_ui_init(void);
void diag_ui_clear(void);
void diag_ui_puts(const char *s);
void diag_ui_hex32(const char *label, uint32_t value);
void diag_ui_hex8x5(const char *label, const uint8_t value[5]);
void diag_ui_bool(const char *label, int value);
void diag_ui_layout_test(void);
void csi_stage3b_main(void);
void fpga_primitive_test_main(void);
void fpga_stream_activity_probe_main(void);
void fpga_smec_job2_test_main(void);
void fpga_smec_job2_recovery_main(void);
void fpga_smec_stage24_main(void);
void fpga_smec_stage25_main(void);
void csi_stock_parity_main(void);
void csi_stage27a_main(void);
void csi_stage27b_main(void);
void csi_stage29_main(void);

#if CSI_DIAG_STAGE == 1
static void show_snapshot(const struct r11_csi_snapshot *s)
{
    diag_ui_hex32("CCU +064:      ", s->ccu_0064);
    diag_ui_hex32("CCU +100:      ", s->ccu_0100);
    diag_ui_hex32("CCU +134:      ", s->ccu_0134);
    diag_ui_hex32("PE CFG0:       ", s->pe_cfg0);
    diag_ui_hex32("PE CFG1:       ", s->pe_cfg1);
    diag_ui_hex32("PE DATA:       ", s->pe_data);
    diag_ui_hex32("CSI +008:      ", s->reg_0008);
    diag_ui_hex32("CSI +050 BUF0: ", s->reg_0050);
    diag_ui_hex32("CSI +070:      ", s->reg_0070);
    diag_ui_hex32("CSI +074 STAT: ", s->reg_0074);
    diag_ui_hex32("CSI +080:      ", s->reg_0080);
    diag_ui_hex32("CSI +084:      ", s->reg_0084);
    diag_ui_hex32("CSI +088:      ", s->reg_0088);
}

#endif

void main(void)
{
#if CSI_DIAG_STAGE == 29
    csi_stage29_main();
#elif CSI_DIAG_STAGE == 28
    csi_stage27b_main();
#elif CSI_DIAG_STAGE == 27
    csi_stage27a_main();
#elif CSI_DIAG_STAGE == 26
    csi_stock_parity_main();
#elif CSI_DIAG_STAGE == 25
    fpga_smec_stage25_main();
#elif CSI_DIAG_STAGE == 24
    fpga_smec_stage24_main();
#elif CSI_DIAG_STAGE == 23
    fpga_smec_job2_recovery_main();
#elif CSI_DIAG_STAGE == 22
    fpga_smec_job2_test_main();
#elif CSI_DIAG_STAGE == 21
    csi_stage3b_main();
#elif CSI_DIAG_STAGE == 19
    fpga_stream_activity_probe_main();
#else
#if CSI_DIAG_STAGE == 9
    fpga_primitive_test_main();
#elif CSI_DIAG_STAGE == 8
    csi_stage3b_main();
#elif CSI_DIAG_STAGE == 7
    csi_stage3b_main();
#elif CSI_DIAG_STAGE == 6
    diag_ui_init();
    diag_ui_layout_test();
    for (;;)
        __asm__ volatile ("wfi");
#else
    struct fpga_diag_result fpga;

#if CSI_DIAG_STAGE >= 1
    struct r11_csi_snapshot csi;
#endif

#if CSI_DIAG_STAGE >= 2
    struct r11_csi_init_result init;
    int init_ok;
#endif
#if CSI_DIAG_STAGE >= 4
    struct r11_csi_config_result config;
    int config_ok;
#endif
#if CSI_DIAG_STAGE >= 5
    struct fpga_capture_arm_probe_result arm_probe;
#endif

    diag_ui_init();
    diag_ui_clear();
    diag_ui_puts("DPOS350P CSI DIAGNOSTIC\n");
    diag_ui_hex32("STAGE:          ", CSI_DIAG_STAGE);
    diag_ui_bool("D-CACHE:        ", cache_dcache_enabled());
    diag_ui_puts("\n");

    fpga_diag_read_id(&fpga);
    diag_ui_hex8x5("FPGA RAW5:     ", fpga.raw);
    diag_ui_hex32("FPGA ID:        ", fpga.id_word_shifted);
    diag_ui_bool("FPGA PRESENT:   ", fpga.present);

#if CSI_DIAG_STAGE >= 5
    diag_ui_puts("\nFPGA 0x08 ARM PROBE\n");
    fpga_capture_arm_probe(&arm_probe);
    diag_ui_puts("SEQUENCE: 08/00/08/01\n");
    diag_ui_hex32("PE CFG0 BEFORE: ", arm_probe.pe_cfg0_before);
    diag_ui_hex32("PE CFG0 AFTER:  ", arm_probe.pe_cfg0_after);
    diag_ui_hex32("PE CFG1 BEFORE: ", arm_probe.pe_cfg1_before);
    diag_ui_hex32("PE CFG1 AFTER:  ", arm_probe.pe_cfg1_after);
    diag_ui_hex32("PE DATA BEFORE: ", arm_probe.pe_data_before);
    diag_ui_hex32("PE DATA AFTER:  ", arm_probe.pe_data_after);
    diag_ui_puts("BANK SELECT:     NO\n");
    diag_ui_puts("CSI MODE:        STILL OFF\n");
#endif

#if CSI_DIAG_STAGE >= 2
    diag_ui_puts("\nCSI BLOCK ACCESS PREPARE\n");
    init_ok = r11_csi_prepare_no_capture(0x42000040U, &init);
    diag_ui_bool("CLOCK PREPARE:  ", init_ok);
    diag_ui_bool("PLL READY:      ", init.pll_ready);
    diag_ui_hex32("CCU 064 BEFORE: ", init.ccu_0064_before);
    diag_ui_hex32("CCU 064 AFTER:  ", init.ccu_0064_after);
    diag_ui_hex32("CCU 2C4 BEFORE: ", init.ccu_02c4_before);
    diag_ui_hex32("CCU 2C4 AFTER:  ", init.ccu_02c4_after);
    diag_ui_hex32("CCU 02C BEFORE: ", init.ccu_002c_before);
    diag_ui_hex32("CCU 02C AFTER:  ", init.ccu_002c_after);
    diag_ui_hex32("CCU 100 BEFORE: ", init.ccu_0100_before);
    diag_ui_hex32("CCU 100 AFTER:  ", init.ccu_0100_after);
    diag_ui_hex32("CCU 134 BEFORE: ", init.ccu_0134_before);
    diag_ui_hex32("CCU 134 AFTER:  ", init.ccu_0134_after);
    diag_ui_hex32("PE CFG0 BEFORE: ", init.pe_cfg0_before);
    diag_ui_hex32("PE CFG0 AFTER:  ", init.pe_cfg0_after);
    diag_ui_hex32("PE CFG1 BEFORE: ", init.pe_cfg1_before);
    diag_ui_hex32("PE CFG1 AFTER:  ", init.pe_cfg1_after);
    diag_ui_hex32("BUF EXPECTED:   ", init.buffer_expected);
    diag_ui_hex32("BUF READBACK:   ", init.buffer_readback);
    diag_ui_bool("BUF READBACK OK:",
                 init.buffer_readback == init.buffer_expected);
    r11_csi_snapshot(&csi);
    diag_ui_hex32("CSI +008:      ", csi.reg_0008);
    diag_ui_hex32("CSI +050:      ", csi.reg_0050);
    diag_ui_hex32("CSI +070:      ", csi.reg_0070);
    diag_ui_hex32("CSI +074:      ", csi.reg_0074);
    diag_ui_hex32("CSI +080:      ", csi.reg_0080);
    diag_ui_hex32("CSI +084:      ", csi.reg_0084);
    diag_ui_hex32("CSI +088:      ", csi.reg_0088);
#if CSI_DIAG_STAGE >= 4
    diag_ui_puts("\nEXPLICIT CSI CONFIG\n");
    config_ok = r11_csi_configure_no_capture(&config);
    diag_ui_bool("CONFIG WRITE:   ", config_ok);
    diag_ui_hex32("GEO0 EXPECTED:  ", config.geo0_expected);
    diag_ui_hex32("GEO0 READBACK:  ", config.geo0_readback);
    diag_ui_hex32("GEO1 EXPECTED:  ", config.geo1_expected);
    diag_ui_hex32("GEO1 READBACK:  ", config.geo1_readback);
    diag_ui_hex32("GEO2 EXPECTED:  ", config.geo2_expected);
    diag_ui_hex32("GEO2 READBACK:  ", config.geo2_readback);
    diag_ui_hex32("IRQ EXPECTED:   ", config.irq_expected);
    diag_ui_hex32("IRQ READBACK:   ", config.irq_readback);
    diag_ui_hex32("MODE READBACK:  ", config.mode_readback);
    diag_ui_bool("CAPTURE OFF:    ", config.capture_disabled);
#endif
    diag_ui_puts("CAPTURE ENABLE: NO\n");
    diag_ui_puts("FPGA STREAM:    NOT STARTED\n");
#elif CSI_DIAG_STAGE >= 1
    diag_ui_puts("\nPASSIVE CSI SNAPSHOT\n");
    r11_csi_snapshot(&csi);
    show_snapshot(&csi);
    diag_ui_puts("\nNO CSI WRITES PERFORMED\n");
#else
    diag_ui_puts("\nBASELINE ONLY\n");
#endif

    diag_ui_puts("POWER CYCLE TO EXIT\n");
    for (;;)
        __asm__ volatile ("wfi");
#endif
#endif
}
