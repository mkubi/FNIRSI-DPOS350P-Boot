#include <stdint.h>
#include "../../drivers/fpga_bus.h"
#include "../../drivers/r11_csi.h"

#define STOCK_RAW_BANK0 0x42DF76D4U

/*
 * Stock 0x40065EF8 owns only PE0, PE2 and PE3 in PE_CFG0.
 * PE1 and PE4..PE7 retain their pre-existing board/application state.
 */
#define CSI_CFG0_OWNED_MASK     0x0000FF0FU
#define CSI_CFG0_OWNED_EXPECTED 0x00002202U

/* Stock control-mode restore owns PE0..PE3. */
#define GPIO_CFG0_CONTROL_MASK      0x0000FFFFU
#define GPIO_CFG0_CONTROL_EXPECTED  0x00001111U
#define GPIO_DATA_IDLE_MASK         0x00000003U
#define GPIO_DATA_IDLE_EXPECTED 0x00000001U

void diag_ui_init(void);
void diag_ui_clear(void);
void diag_ui_text_at(unsigned int column, unsigned int row,
                     const char *text);
void diag_ui_hex32_at(unsigned int column, unsigned int row,
                      const char *label, uint32_t value);
void diag_ui_bool_at(unsigned int column, unsigned int row,
                     const char *label, int value);

static int value_is(uint32_t value, uint32_t expected)
{
    return value == expected;
}

static int csi_pinmux_matches(uint32_t cfg0, uint32_t cfg1)
{
    return
        (cfg0 & CSI_CFG0_OWNED_MASK) == CSI_CFG0_OWNED_EXPECTED &&
        cfg1 == 0x22222222U;
}

static int gpio_restore_matches(uint32_t cfg0,
                                uint32_t cfg1,
                                uint32_t data)
{
    return
        (cfg0 & GPIO_CFG0_CONTROL_MASK) == GPIO_CFG0_CONTROL_EXPECTED &&
        cfg1 == 0x00000000U &&
        (data & GPIO_DATA_IDLE_MASK) == GPIO_DATA_IDLE_EXPECTED;
}

void csi_stock_parity_main(void)
{
    struct fpga_diag_result fpga;
    struct r11_csi_stock_parity_result result;
    int run_ok;
    int csi_state_ok;
    int restore_ok;

    /*
     * This application is freestanding. Avoid aggregate zero
     * initialization here because GCC may lower it to memset().
     *
     * Seed fields that may otherwise remain unset on an early
     * clock/PLL failure.
     */
    result.pll_ready = 0;
    result.initial_if_cfg = 0U;
    result.initial_hsize = 0U;
    result.final_if_cfg = 0U;
    result.final_hsize = 0U;
    result.csi_pe_cfg0 = 0U;
    result.csi_pe_cfg1 = 0U;
    result.buffer0 = 0U;
    result.irq_enable = 0U;
    result.mode = 0U;

    diag_ui_init();
    diag_ui_clear();

    /* Keep the known-good Custom App control-plane presence check. */
    fpga_diag_read_id_baseline(&fpga);

    run_ok = r11_csi_stock_parity_init(STOCK_RAW_BANK0, &result);

    csi_state_ok =
        result.pll_ready &&
        value_is(result.initial_if_cfg, 0x01050000U) &&
        value_is(result.initial_hsize, 0x00300200U) &&
        value_is(result.final_if_cfg, 0x01060400U) &&
        value_is(result.final_hsize, 0x00000A00U) &&
        csi_pinmux_matches(result.csi_pe_cfg0, result.csi_pe_cfg1) &&
        value_is(result.buffer0, STOCK_RAW_BANK0 >> 2) &&
        value_is(result.irq_enable, R11_CSI_DONE) &&
        value_is(result.mode, 0U);

    /* Restore the shared PE bus even when an earlier parity check failed. */
    fpga_bus_safe_idle();
    r11_csi_snapshot(&result.restored);

    restore_ok = gpio_restore_matches(result.restored.pe_cfg0,
                                      result.restored.pe_cfg1,
                                      result.restored.pe_data);

    diag_ui_text_at(2U, 1U, "CUSTOM CSI STOCK PARITY - STAGE 26");
    diag_ui_text_at(2U, 3U, "CLOCK / CONFIG");
    diag_ui_text_at(66U, 3U, "PINMUX / FINAL / RESTORE");

    diag_ui_bool_at(2U, 5U, "FPGA ID:       ", fpga.present);
    diag_ui_bool_at(2U, 6U, "PLL READY:     ", result.pll_ready);
    diag_ui_hex32_at(2U, 8U, "INIT IF:       ", result.initial_if_cfg);
    diag_ui_bool_at(2U, 9U, "INIT IF MATCH: ",
                    value_is(result.initial_if_cfg, 0x01050000U));
    diag_ui_hex32_at(2U, 10U, "INIT HSIZE:    ", result.initial_hsize);
    diag_ui_bool_at(2U, 11U, "INIT HS MATCH: ",
                    value_is(result.initial_hsize, 0x00300200U));
    diag_ui_hex32_at(2U, 13U, "FINAL IF:      ", result.final_if_cfg);
    diag_ui_bool_at(2U, 14U, "FINAL IF MATCH:",
                    value_is(result.final_if_cfg, 0x01060400U));
    diag_ui_hex32_at(2U, 15U, "FINAL HSIZE:   ", result.final_hsize);
    diag_ui_bool_at(2U, 16U, "FINAL HS MATCH:",
                    value_is(result.final_hsize, 0x00000A00U));

    diag_ui_hex32_at(66U, 5U, "CSI CFG0:      ", result.csi_pe_cfg0);
    diag_ui_hex32_at(66U, 6U, "CSI CFG1:      ", result.csi_pe_cfg1);
    diag_ui_bool_at(66U, 7U, "PINMUX MATCH:  ",
                    csi_pinmux_matches(result.csi_pe_cfg0, result.csi_pe_cfg1));
    diag_ui_hex32_at(66U, 9U, "BUF0:          ", result.buffer0);
    diag_ui_bool_at(66U, 10U, "BUF0 MATCH:    ",
                    value_is(result.buffer0, STOCK_RAW_BANK0 >> 2));
    diag_ui_hex32_at(66U, 11U, "IRQ ENABLE:    ", result.irq_enable);
    diag_ui_bool_at(66U, 12U, "IRQ MATCH:     ",
                    value_is(result.irq_enable, R11_CSI_DONE));
    diag_ui_hex32_at(66U, 14U, "RESTORE CFG0:  ", result.restored.pe_cfg0);
    diag_ui_hex32_at(66U, 15U, "RESTORE CFG1:  ", result.restored.pe_cfg1);
    diag_ui_hex32_at(66U, 16U, "RESTORE DATA:  ", result.restored.pe_data);

    diag_ui_bool_at(2U, 19U, "CSI PARITY:    ", run_ok && csi_state_ok);
    diag_ui_bool_at(2U, 20U, "GPIO RESTORE:  ", restore_ok);
    diag_ui_bool_at(2U, 22U, "OVERALL:       ",
                    fpga.present && run_ok && csi_state_ok && restore_ok);
    diag_ui_text_at(2U, 25U, "NO CAPTURE - NO SMEC - POWER CYCLE TO EXIT");

    for (;;)
        __asm__ volatile ("wfi");
}
