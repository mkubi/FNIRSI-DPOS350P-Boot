#include <stdint.h>

#include "custom_fpga_start.h"
#include "../../drivers/fpga_bus.h"
#include "../../drivers/r11_csi.h"
#include "../../drivers/ms5351_stock.h"

#define PE_CFG0        (*(volatile uint32_t *)0x01C20890U)
#define PE_CFG1        (*(volatile uint32_t *)0x01C20894U)
#define PE_DATA        (*(volatile uint32_t *)0x01C208A0U)
#define CSI_IF_CFG     (*(volatile uint32_t *)0x01CB4000U)
#define CSI_MODE       (*(volatile uint32_t *)0x01CB4008U)
#define CSI_HSIZE      (*(volatile uint32_t *)0x01CB4080U)
#define CSI_BUFFER0    (*(volatile uint32_t *)0x01CB4050U)
#define CSI_IRQ_ENABLE (*(volatile uint32_t *)0x01CB4070U)
#define RAW_BANK0      0x42DF76D4U

#define FPGA_BUS_DIRECTION_STATE (*(volatile uint8_t *)0x42E68D18U)
#define FPGA_INTERFACE_MODE_STATE (*(volatile uint8_t *)0x42E68D19U)

#define STOCK_F1_CSI_INIT   ((void (*)(uint32_t))0x400660C8U)
#define STOCK_F3_GPIO_MODE  ((void (*)(uint32_t))0x40049094U)
#define STOCK_F4_CLOCK_TAIL ((void (*)(void))0x400490B8U)

volatile struct dposlab_custom_fpga_start_result
    dposlab_custom_fpga_start_result;

const char *dposlab_fpga_impl_name(unsigned impl)
{
    switch (impl) {
    case FPGA_IMPL_STOCK:  return "STOCK";
    case FPGA_IMPL_CUSTOM: return "CUSTOM";
    case FPGA_IMPL_SKIP:   return "SKIP";
    case FPGA_IMPL_EXACT:  return "EXACT";
    default:               return "INVALID";
    }
}

static int phase_f1(struct r11_csi_stock_parity_result *parity)
{
    (void)parity;
#if FPGA_F1 == FPGA_IMPL_STOCK
    STOCK_F1_CSI_INIT(0x1F40U);
    return 1;
#elif FPGA_F1 == FPGA_IMPL_CUSTOM
    return r11_csi_stock_parity_init(RAW_BANK0, parity);
#elif FPGA_F1 == FPGA_IMPL_SKIP
    (void)parity;
    return 1;
#else
#error "FPGA_F1 supports STOCK, CUSTOM or SKIP"
#endif
}

static void phase_f2(void)
{
#if FPGA_F2 == FPGA_IMPL_EXACT || FPGA_F2 == FPGA_IMPL_STOCK
    FPGA_INTERFACE_MODE_STATE = 1U;
#elif FPGA_F2 == FPGA_IMPL_CUSTOM
    /* No independent custom implementation exists yet. Keep this explicit. */
    FPGA_INTERFACE_MODE_STATE = 1U;
#elif FPGA_F2 == FPGA_IMPL_SKIP
    /* Deliberately omitted. */
#else
#error "FPGA_F2 supports STOCK, CUSTOM, EXACT or SKIP"
#endif
}

static void phase_f3(void)
{
#if FPGA_F3 == FPGA_IMPL_STOCK
    STOCK_F3_GPIO_MODE(0U);
#elif FPGA_F3 == FPGA_IMPL_CUSTOM
    fpga_bus_safe_idle();
#elif FPGA_F3 == FPGA_IMPL_SKIP
    /* Deliberately omitted. */
#else
#error "FPGA_F3 supports STOCK, CUSTOM or SKIP"
#endif
}

static int phase_f4(struct ms5351_stock_result *clock)
{
    (void)clock;
#if FPGA_F4 == FPGA_IMPL_STOCK
    STOCK_F4_CLOCK_TAIL();
    return 1;
#elif FPGA_F4 == FPGA_IMPL_CUSTOM
    ms5351_stock_init_95m_10m(clock);
    return clock->completed;
#elif FPGA_F4 == FPGA_IMPL_SKIP
    (void)clock;
    return 1;
#else
#error "FPGA_F4 supports STOCK, CUSTOM or SKIP"
#endif
}

void dposlab_custom_fpga_start(void)
{
    struct fpga_diag_result fpga;
    struct r11_csi_stock_parity_result parity;
    struct ms5351_stock_result clock;
    volatile struct dposlab_custom_fpga_start_result *r =
        &dposlab_custom_fpga_start_result;
    int csi_ok;
    int clock_ok;

    /* Avoid compiler-generated memset in the freestanding DPOSLAB image. */
    fpga.present = 0;
    fpga.id_word = 0U;
    fpga.id_word_shifted = 0U;
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
    clock.writes = 0U;
    clock.completed = 0;

    /* Constant diagnostic preamble for every matrix combination. */
    fpga_diag_read_id_baseline(&fpga);

    csi_ok = fpga.present ? phase_f1(&parity) : 0;
    phase_f2();
    phase_f3();
    clock_ok = (fpga.present && csi_ok) ? phase_f4(&clock) : 0;

    r->id = fpga.id_word_shifted == 0xA5A55A5AU ?
        fpga.id_word_shifted : fpga.id_word;
    r->id_ok = fpga.present;
    r->csi_ok = csi_ok;
    r->clock_ok = clock_ok;
    r->clock_writes = clock.writes;
    r->pe_cfg0 = PE_CFG0;
    r->pe_cfg1 = PE_CFG1;
    r->pe_data = PE_DATA;
    r->csi_if_cfg = CSI_IF_CFG;
    r->csi_mode = CSI_MODE;
    r->csi_hsize = CSI_HSIZE;
    r->csi_buffer0 = CSI_BUFFER0;
    r->csi_irq_enable = CSI_IRQ_ENABLE;
    r->bus_direction_state = FPGA_BUS_DIRECTION_STATE;
    r->interface_mode_state = FPGA_INTERFACE_MODE_STATE;
    r->f1 = (uint8_t)FPGA_F1;
    r->f2 = (uint8_t)FPGA_F2;
    r->f3 = (uint8_t)FPGA_F3;
    r->f4 = (uint8_t)FPGA_F4;
    r->success = fpga.present && csi_ok && clock_ok;
}
