#include "../../drivers/fpga_bus_ro.h"

void diag_ui_init(void);
void diag_ui_fpga_ab_screen(const struct fpga_diag_result *baseline,
                            const struct fpga_diag_result *stock);

void fpga_primitive_test_main(void)
{
    struct fpga_diag_result baseline;
    struct fpga_diag_result stock;

    diag_ui_init();
    fpga_diag_read_id_baseline(&baseline);
    fpga_diag_read_id(&stock);
    diag_ui_fpga_ab_screen(&baseline, &stock);

    for (;;)
        __asm__ volatile ("wfi");
}
