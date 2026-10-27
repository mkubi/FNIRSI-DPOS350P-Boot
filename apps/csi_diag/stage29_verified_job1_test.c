#include <stdint.h>
#include "../../drivers/fpga_bus_ro.h"
#include "../../drivers/r11_csi.h"
#include "../../drivers/ms5351_stock.h"

void diag_ui_init(void);
void diag_ui_clear(void);
void diag_ui_text_at(unsigned int column, unsigned int row,
                     const char *text);
void diag_ui_hex32_at(unsigned int column, unsigned int row,
                      const char *label, uint32_t value);
void diag_ui_bool_at(unsigned int column, unsigned int row,
                     const char *label, int value);

void csi_stage29_main(void)
{
    struct fpga_diag_result fpga;
    struct r11_csi_stock_parity_result parity;
    struct ms5351_stock_result clock;
    struct fpga_smec_stage24_pe_result trace;
    struct fpga_smec_init_result all_jobs;
    int parity_ok;
    int fpga_start_ok;

    parity.pll_ready = 0;
    parity.initial_if_cfg = 0U;
    parity.initial_hsize = 0U;
    parity.final_if_cfg = 0U;
    parity.final_hsize = 0U;
    parity.csi_pe_cfg0 = 0U;
    parity.csi_pe_cfg1 = 0U;
    parity.buffer0 = 0U;
    parity.irq_enable = 0U;
    parity.mode = 0U;
    clock.completed = 0;
    all_jobs.completed_jobs = 0U;
    all_jobs.attempts = 0U;
    all_jobs.poll64_count = 0U;
    all_jobs.poll65_count = 0U;
    all_jobs.result66 = 0U;
    all_jobs.timing = 0U;
    all_jobs.current_job = 0U;
    all_jobs.status64 = 0U;
    all_jobs.final65 = 0U;
    all_jobs.echo68 = 0U;
    all_jobs.status67 = 0U;
    all_jobs.success = 0;
    all_jobs.error = 0;

    diag_ui_init();
    diag_ui_clear();

    fpga_diag_read_id_baseline(&fpga);

    /* Reproduce the currently confirmed FPGA_Start prerequisite sequence. */
    parity_ok = fpga.present ?
        r11_csi_stock_parity_init(0x42DF76D4U, &parity) : 0;
    fpga_bus_safe_idle();

    if (fpga.present && parity_ok)
        ms5351_stock_init_95m_10m(&clock);

    fpga_start_ok = fpga.present && parity_ok && clock.completed;

    /*
     * End-to-end Custom App validation after repairing all three PE bus
     * primitives. First preserve the detailed Job 1/2 trace, then execute
     * the complete confirmed Custom startup set Jobs 1..7.
     * No acquisition probe, capture, or DMA is started here.
     */
    if (fpga_start_ok) {
        fpga_smec_stage29_verified_job1_test(&trace);
        if (trace.job1.completed != 0U && trace.job2.completed != 0U)
            (void)fpga_smec_init_baseline(&all_jobs);
    } else {
        trace.job1.completed = 0U;
        trace.job2.completed = 0U;
    }

    diag_ui_text_at(2U, 1U, "STAGE 29 - JOBS 1-7 WITHOUT SYSTEM_INIT PART2");
    diag_ui_text_at(2U, 3U, "PREREQUISITES");
    diag_ui_text_at(66U, 3U, "CUSTOM JOBS 1-7 VALIDATION");

    diag_ui_bool_at(2U, 5U, "FPGA ID:       ", fpga.present);
    diag_ui_bool_at(2U, 6U, "CSI PARITY:    ", parity_ok);
    diag_ui_bool_at(2U, 7U, "CLOCK CHAIN:   ", clock.completed);
    diag_ui_bool_at(2U, 8U, "FPGA START:    ", fpga_start_ok);

    diag_ui_hex32_at(2U, 10U, "PE CFG0:       ", trace.cfg0_after);
    diag_ui_hex32_at(2U, 11U, "PE CFG1:       ", trace.cfg1_after);
    diag_ui_hex32_at(2U, 12U, "PE PULL0:      ", trace.pull0_after);
    diag_ui_hex32_at(2U, 13U, "PE DATA:       ", trace.data_after);

    diag_ui_bool_at(66U, 5U, "JOB1 COMPLETE: ", trace.job1.completed != 0U);
    diag_ui_hex32_at(66U, 6U, "J1 ERROR:      ", (uint32_t)trace.job1.error);
    diag_ui_hex32_at(66U, 7U, "J1 P64 OR:     ", trace.job1.poll64.or_value);
    diag_ui_hex32_at(66U, 8U, "J1 P64 COUNT:  ", trace.job1.poll64.count);
    diag_ui_hex32_at(66U, 9U, "J1 P65 OR:     ", trace.job1.poll65.or_value);
    diag_ui_hex32_at(66U, 10U, "J1 P65 COUNT:  ", trace.job1.poll65.count);

    diag_ui_bool_at(66U, 12U, "JOB2 COMPLETE: ", trace.job2.completed != 0U);
    diag_ui_hex32_at(66U, 13U, "J2 ERROR:      ", (uint32_t)trace.job2.error);
    diag_ui_hex32_at(66U, 14U, "J2 P64 FIRST:  ", trace.job2.poll64.first);
    diag_ui_hex32_at(66U, 15U, "J2 P64 LAST:   ", trace.job2.poll64.last);
    diag_ui_hex32_at(66U, 16U, "J2 P64 OR:     ", trace.job2.poll64.or_value);
    diag_ui_hex32_at(66U, 17U, "J2 P64 COUNT:  ", trace.job2.poll64.count);
    diag_ui_hex32_at(66U, 18U, "J2 P65 FIRST:  ", trace.job2.poll65.first);
    diag_ui_hex32_at(66U, 19U, "J2 P65 OR:     ", trace.job2.poll65.or_value);
    diag_ui_hex32_at(66U, 20U, "J2 ECHO/STAT:  ",
                     ((uint32_t)trace.job2.echo68 << 8) |
                     (uint32_t)trace.job2.status67);

    diag_ui_hex32_at(66U, 21U, "JOBS COMPLETE: ",
                     all_jobs.completed_jobs);
    diag_ui_bool_at(66U, 22U, "ALL 7 PASS:    ",
                    all_jobs.success != 0 &&
                    all_jobs.completed_jobs == 7U);

    diag_ui_text_at(2U, 15U, "PART2:          SKIPPED (A/B CONTROL)");
    diag_ui_text_at(2U, 17U, "CAPTURE NOTE:   RESTORE PART2 IF NEEDED");
    diag_ui_hex32_at(2U, 18U, "FINAL JOB:     ", all_jobs.current_job);
    diag_ui_hex32_at(2U, 19U, "FINAL RESULT66:", all_jobs.result66);
    diag_ui_hex32_at(2U, 20U, "FINAL ECHO/ST: ",
                     ((uint32_t)all_jobs.echo68 << 8) |
                     (uint32_t)all_jobs.status67);
    diag_ui_hex32_at(2U, 21U, "FINAL ERROR:   ",
                     (uint32_t)all_jobs.error);
    diag_ui_text_at(2U, 23U,
                    "FIXED CUSTOM BUS - PART2 OFF - JOBS 1 THROUGH 7");

    for (;;)
        __asm__ volatile ("wfi");
}
