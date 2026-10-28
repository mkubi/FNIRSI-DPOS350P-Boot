#include "../../drivers/fpga_bus.h"
#include "../../drivers/ms5351_stock.h"

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


void diag_ui_acq_ready_screen(const struct fpga_diag_result *fpga,
                              const struct fpga_acq_ready_result *probe);

void fpga_acq_ready_probe_main(void)
{
    struct fpga_diag_result fpga;
    struct fpga_acq_ready_result probe;

    diag_ui_init();
    fpga_diag_read_id_baseline(&fpga);

    /* 4 is the locally reconstructed counter-driven acquisition mode byte.
     * This stage performs no CSI access and remains bounded at 2500 polls,
     * matching the stock inner readiness window. */
    fpga_acq_ready_probe(4U, 2500U, &probe);
    diag_ui_acq_ready_screen(&fpga, &probe);

    for (;;)
        __asm__ volatile ("wfi");
}


void diag_ui_acq_position_screen(const struct fpga_diag_result *fpga,
                                 const struct fpga_acq_position_result *probe);

void fpga_acq_position_probe_main(void)
{
    struct fpga_diag_result fpga;
    struct fpga_acq_position_result probe;

    diag_ui_init();
    fpga_diag_read_id_baseline(&fpga);
    fpga_acq_position_probe(4U, 2500U, &probe);
    diag_ui_acq_position_screen(&fpga, &probe);

    for (;;)
        __asm__ volatile ("wfi");
}


void diag_ui_ms5351_screen(const struct fpga_diag_result *before,
                           const struct fpga_diag_result *after,
                           const struct ms5351_stock_result *clock);

void ms5351_clock_probe_main(void)
{
    struct fpga_diag_result before;
    struct fpga_diag_result after;
    struct ms5351_stock_result clock;

    diag_ui_init();
    fpga_diag_read_id_baseline(&before);
    ms5351_stock_init_95m_10m(&clock);
    fpga_diag_read_id_baseline(&after);
    diag_ui_ms5351_screen(&before, &after, &clock);

    for (;;)
        __asm__ volatile ("wfi");
}


void diag_ui_stream_activity_screen(
    const struct fpga_diag_result *fpga,
    const struct fpga_acq_position_result *position,
    const struct fpga_stream_activity_result *activity,
    const struct ms5351_stock_result *clock);

void fpga_stream_activity_probe_main(void)
{
    struct fpga_diag_result fpga;
    struct fpga_acq_position_result position;
    struct fpga_stream_activity_result activity;
    struct ms5351_stock_result clock;

    diag_ui_init();
    ms5351_stock_init_95m_10m(&clock);
    fpga_diag_read_id_baseline(&fpga);
    fpga_acq_position_probe(4U, 2500U, &position);

    if (clock.completed && fpga.present && position.ready.ready &&
        position.valid) {
        fpga_set_stream_quantity(0x2134U);
        fpga_prepare_csi_bank0_position(position.position);
        fpga_stream_activity_probe(4000000U, &activity);
    } else {
        activity.first = 0U;
        activity.last = 0U;
        activity.changed_mask = 0U;
        activity.high_mask = 0U;
        activity.low_mask = 0U;
        activity.samples = 0U;
    }

    diag_ui_stream_activity_screen(&fpga, &position, &activity, &clock);
    for (;;)
        __asm__ volatile ("wfi");
}
