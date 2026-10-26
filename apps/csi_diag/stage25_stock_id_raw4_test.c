#include "../../drivers/fpga_bus_ro.h"

void diag_ui_init(void);
void diag_ui_stage25_raw4_screen(
    const struct fpga_diag_result *fpga,
    const struct fpga_smec_stage24_pe_result *trace);

void fpga_smec_stage25_main(void)
{
    struct fpga_diag_result fpga;
    struct fpga_smec_stage24_pe_result trace;

    diag_ui_init();
    fpga_diag_read_id_stock_raw4(&fpga);
    fpga_smec_stage24_pe_cleanup_test(&trace);
    diag_ui_stage25_raw4_screen(&fpga, &trace);

    for (;;)
        __asm__ volatile ("wfi");
}
