#include "r11_csi.h"
#include "regs.h"
#include "timer.h"
#include "cache.h"

#define CCU_BASE       0x01C20000U
#define PIO_BASE       0x01C20800U
#define PE_CFG0        (PIO_BASE + 0x90U)
#define PE_CFG1        (PIO_BASE + 0x94U)
#define PE_DATA        (PIO_BASE + 0xA0U)

#define CSI_REG_IF_CFG (R11_CSI_BASE + 0x04U)
#define CSI_REG_HSIZE  (R11_CSI_BASE + 0x44U)
#define CSI_REG_MODE   (R11_CSI_BASE + 0x08U)
#define CSI_REG_BUF0   (R11_CSI_BASE + 0x50U)
#define CSI_REG_IRQ_EN (R11_CSI_BASE + 0x70U)
#define CSI_REG_STATUS (R11_CSI_BASE + 0x74U)
#define CSI_REG_GEO0   (R11_CSI_BASE + 0x80U)
#define CSI_REG_GEO1   (R11_CSI_BASE + 0x84U)
#define CSI_REG_GEO2   (R11_CSI_BASE + 0x88U)

static int wait_mask_set(uintptr_t address, uint32_t mask,
                         uint32_t timeout_ms)
{
    uint32_t deadline = timer_get_ms() + timeout_ms;

    while ((mmio_read32(address) & mask) == 0U) {
        if (timer_deadline_reached(deadline))
            return 0;
    }
    return 1;
}

static void csi_pinmux_enable(void)
{
    uint32_t cfg0 = mmio_read32(PE_CFG0);

    /* Stock EXE2 0x40065EF8: PE0, PE2 and PE3 -> function 2. */
    cfg0 &= ~((0xFU << 0) | (0xFU << 8) | (0xFU << 12));
    cfg0 |=  ((2U   << 0) | (2U   << 8) | (2U   << 12));
    mmio_write32(PE_CFG0, cfg0);

    /* Stock path writes PE8..PE15 as function 2. */
    mmio_write32(PE_CFG1, 0x22222222U);
    cache_dsb();
}

int r11_csi_prepare_no_capture(uintptr_t buffer_address,
                               struct r11_csi_init_result *r)
{
    uint32_t value;

    r->ccu_0064_before = mmio_read32(CCU_BASE + 0x64U);
    r->ccu_02c4_before = mmio_read32(CCU_BASE + 0x2C4U);
    r->ccu_002c_before = mmio_read32(CCU_BASE + 0x2CU);
    r->ccu_0100_before = mmio_read32(CCU_BASE + 0x100U);
    r->ccu_0134_before = mmio_read32(CCU_BASE + 0x134U);
    r->pe_cfg0_before = mmio_read32(PE_CFG0);
    r->pe_cfg1_before = mmio_read32(PE_CFG1);

    /* Stock 0x40042914(0x0C885888): CSI bus gate and reset bit 8. */
    value = mmio_read32(CCU_BASE + 0x64U);
    mmio_write32(CCU_BASE + 0x64U, value | (1U << 8));
    value = mmio_read32(CCU_BASE + 0x2C4U);
    mmio_write32(CCU_BASE + 0x2C4U, value | (1U << 8));
    cache_dsb();

    /* Remaining stock EXE2 0x40065F48 module-clock sequence. */
    value = mmio_read32(CCU_BASE + 0x100U);
    mmio_write32(CCU_BASE + 0x100U, value | 0x00000002U);

    value = mmio_read32(CCU_BASE + 0x2CU);
    mmio_write32(CCU_BASE + 0x2CU, value | 0x80000000U);
    r->pll_ready = wait_mask_set(CCU_BASE + 0x2CU, 0x01000000U, 100U);

    if (!r->pll_ready)
        return 0;

    mmio_write32(CCU_BASE + 0x134U, 0x81800000U);
    cache_dsb();

    csi_pinmux_enable();

    /* Destination programming only. Capture mode remains disabled. */
    r11_csi_set_buffer0(buffer_address);

    r->ccu_0064_after = mmio_read32(CCU_BASE + 0x64U);
    r->ccu_02c4_after = mmio_read32(CCU_BASE + 0x2C4U);
    r->ccu_002c_after = mmio_read32(CCU_BASE + 0x2CU);
    r->ccu_0100_after = mmio_read32(CCU_BASE + 0x100U);
    r->ccu_0134_after = mmio_read32(CCU_BASE + 0x134U);
    r->pe_cfg0_after = mmio_read32(PE_CFG0);
    r->pe_cfg1_after = mmio_read32(PE_CFG1);
    r->buffer_expected = (uint32_t)(buffer_address >> 2);
    r->buffer_readback = r11_csi_buffer0();
    r->buffer_ok = r->buffer_readback == r->buffer_expected;
    return 1;
}

void r11_csi_snapshot(struct r11_csi_snapshot *s)
{
    s->ccu_0064 = mmio_read32(CCU_BASE + 0x64U);
    s->ccu_0100 = mmio_read32(CCU_BASE + 0x100U);
    s->ccu_0134 = mmio_read32(CCU_BASE + 0x134U);
    s->pe_cfg0 = mmio_read32(PE_CFG0);
    s->pe_cfg1 = mmio_read32(PE_CFG1);
    s->pe_data = mmio_read32(PE_DATA);
    s->reg_0008 = mmio_read32(CSI_REG_MODE);
    s->reg_0050 = mmio_read32(CSI_REG_BUF0);
    s->reg_0070 = mmio_read32(CSI_REG_IRQ_EN);
    s->reg_0074 = mmio_read32(CSI_REG_STATUS);
    s->reg_0080 = mmio_read32(CSI_REG_GEO0);
    s->reg_0084 = mmio_read32(CSI_REG_GEO1);
    s->reg_0088 = mmio_read32(CSI_REG_GEO2);
}

int r11_csi_configure_no_capture(struct r11_csi_config_result *r)
{
    /*
     * Stage 2C exposed the stock/reset geometry currently used by the
     * enabled CSI block. Stage 3A writes those values explicitly so the
     * first capture will not depend on inherited or reset state.
     * Capture mode remains zero throughout this test.
     */
    r->geo0_expected = 0x05000000U;
    r->geo1_expected = 0x01E00000U;
    r->geo2_expected = 0x01400280U;
    r->irq_expected = R11_CSI_DONE;

    mmio_write32(CSI_REG_MODE, 0U);
    mmio_write32(CSI_REG_IF_CFG, 0x00060000U);
    mmio_write32(CSI_REG_HSIZE,  0x00000800U);
    mmio_write32(CSI_REG_GEO0, r->geo0_expected);
    mmio_write32(CSI_REG_GEO1, r->geo1_expected);
    mmio_write32(CSI_REG_GEO2, r->geo2_expected);
    mmio_write32(CSI_REG_IRQ_EN, r->irq_expected);
    cache_dsb();

    r->geo0_readback = mmio_read32(CSI_REG_GEO0);
    r->geo1_readback = mmio_read32(CSI_REG_GEO1);
    r->geo2_readback = mmio_read32(CSI_REG_GEO2);
    r->irq_readback = mmio_read32(CSI_REG_IRQ_EN);
    r->mode_readback = mmio_read32(CSI_REG_MODE);
    r->capture_disabled = r->mode_readback == 0U;

    return r->geo0_readback == r->geo0_expected &&
           r->geo1_readback == r->geo1_expected &&
           r->geo2_readback == r->geo2_expected &&
           (r->irq_readback & R11_CSI_DONE) != 0U &&
           r->capture_disabled;
}

uint32_t r11_csi_status(void)
{
    return mmio_read32(CSI_REG_STATUS);
}

void r11_csi_ack(uint32_t mask)
{
    mmio_write32(CSI_REG_STATUS, mask);
    cache_dsb();
}

void r11_csi_set_buffer0(uintptr_t address)
{
    mmio_write32(CSI_REG_BUF0, (uint32_t)(address >> 2));
    cache_dsb();
}

uint32_t r11_csi_buffer0(void)
{
    return mmio_read32(CSI_REG_BUF0);
}

void r11_csi_capture_enable(void)
{
    mmio_write32(CSI_REG_MODE, 1U);
    cache_dsb();
}

void r11_csi_capture_disable(void)
{
    mmio_write32(CSI_REG_MODE, 0U);
    cache_dsb();
}

uint32_t r11_csi_mode(void)
{
    return mmio_read32(CSI_REG_MODE);
}

int r11_csi_wait_done(uint32_t timeout_ms, uint32_t *last_status)
{
    uint32_t deadline = timer_get_ms() + timeout_ms;
    uint32_t status;

    do {
        status = r11_csi_status();
        if ((status & R11_CSI_DONE) != 0U) {
            *last_status = status;
            return 1;
        }
    } while (!timer_deadline_reached(deadline));

    *last_status = status;
    return 0;
}


int r11_csi_wait_done_bounded(uint32_t iterations, uint32_t *last_status)
{
    uint32_t status = 0U;
    while (iterations-- != 0U) {
        status = r11_csi_status();
        if ((status & R11_CSI_DONE) != 0U) {
            *last_status = status;
            return 1;
        }
    }
    *last_status = status;
    return 0;
}

void r11_csi_timer0_gate(int enable)
{
    const uintptr_t timer0_ctrl = 0x01C20C10U;
    uint32_t value = mmio_read32(timer0_ctrl);
    if (enable)
        value |= 1U;
    else
        value &= ~1U;
    mmio_write32(timer0_ctrl, value);
    cache_dsb();
}
