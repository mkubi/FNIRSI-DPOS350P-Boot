#include "../../drivers/fpga_bus_ro.h"

void diag_ui_init(void);
void diag_ui_job2_recovery_screen(const struct fpga_diag_result *fpga,
                                  const struct fpga_smec_job2_recovery *trace);

void fpga_smec_job2_recovery_main(void)
{
    struct fpga_diag_result fpga;
    struct fpga_smec_job2_recovery trace;

    diag_ui_init();
    fpga_diag_read_id_baseline(&fpga);
    fpga_smec_debug_job2_recovery(&trace);
    diag_ui_job2_recovery_screen(&fpga, &trace);
    for (;;)
        __asm__ volatile ("wfi");
}
