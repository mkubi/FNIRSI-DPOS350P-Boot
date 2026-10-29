#include "fpga_init.h"
#include "fpga_bus.h"
#include "ms5351.h"
#include "r11_csi.h"

#define FPGA_INIT_CAPTURE_BUFFER 0x42DF76D4U
#define FPGA_ID_MAGIC          0xA5A55A5AU

static uint32_t select_id(const struct fpga_diag_result *id)
{
    if (id->id_word_shifted == FPGA_ID_MAGIC)
        return id->id_word_shifted;
    return id->id_word;
}

int fpga_init(struct fpga_init_result *result)
{
    struct fpga_init_result local;
    struct fpga_init_result *out = result ? result : &local;
    struct fpga_diag_result before;
    struct fpga_diag_result after;
    struct r11_csi_stock_parity_result csi;
    struct ms5351_init_result clock;

    out->fpga_id_before = 0U;
    out->fpga_id_after = 0U;
    out->clock_writes = 0U;
    out->csi_if_cfg = 0U;
    out->csi_hsize = 0U;
    out->csi_buffer0 = 0U;
    out->csi_irq_enable = 0U;
    out->fpga_before = 0;
    out->csi_ready = 0;
    out->clocks_ready = 0;
    out->fpga_after = 0;

    before.present = 0;
    before.id_word = 0U;
    before.id_word_shifted = 0U;
    after.present = 0;
    after.id_word = 0U;
    after.id_word_shifted = 0U;

    csi.pll_ready = 0;
    csi.initial_if_cfg = 0U;
    csi.initial_hsize = 0U;
    csi.final_if_cfg = 0U;
    csi.final_hsize = 0U;
    csi.csi_pe_cfg0 = 0U;
    csi.csi_pe_cfg1 = 0U;
    csi.buffer0 = 0U;
    csi.irq_enable = 0U;
    csi.mode = 0U;

    clock.writes = 0U;
    clock.completed = 0;

    fpga_diag_read_id_baseline(&before);
    out->fpga_before = before.present;
    out->fpga_id_before = select_id(&before);
    if (!out->fpga_before)
        return 0;

    out->csi_ready = r11_csi_stock_parity_init(
        FPGA_INIT_CAPTURE_BUFFER, &csi);

    /* F2 is deliberately omitted in the confirmed minimal configuration. */
    fpga_bus_safe_idle();

    if (out->csi_ready) {
        ms5351_init_95m_10m(&clock);
        out->clocks_ready = clock.completed;
        out->clock_writes = clock.writes;
    }

    out->csi_if_cfg = csi.final_if_cfg;
    out->csi_hsize = csi.final_hsize;
    out->csi_buffer0 = csi.buffer0;
    out->csi_irq_enable = csi.irq_enable;

    fpga_diag_read_id_baseline(&after);
    out->fpga_after = after.present;
    out->fpga_id_after = select_id(&after);

    return out->fpga_before && out->csi_ready &&
           out->clocks_ready && out->fpga_after;
}
