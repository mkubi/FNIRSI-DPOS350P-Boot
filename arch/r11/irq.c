#include <stdint.h>
#include "irq.h"

#define GIC_BASE             0x01C80000U
#define GICD_CTLR            (GIC_BASE + 0x1000U)
#define GICD_TYPER           (GIC_BASE + 0x1004U)
#define GICD_ISENABLER       (GIC_BASE + 0x1100U)
#define GICD_ICENABLER       (GIC_BASE + 0x1180U)
#define GICD_IPRIORITYR      (GIC_BASE + 0x1400U)
#define GICD_ITARGETSR       (GIC_BASE + 0x1800U)
#define GICD_ICFGR           (GIC_BASE + 0x1C00U)
#define GICC_CTLR            (GIC_BASE + 0x2000U)
#define GICC_PMR             (GIC_BASE + 0x2004U)

#define STOCK_IRQ_STATE      0x42E68BF0U
#define STOCK_HANDLER_TABLE  0x42E67954U
#define STOCK_DEFAULT_ISR    0x400668E0U
#define STOCK_IRQ_COUNT      0x9CU
#define STOCK_DEFAULT_PRIO   0xA0A0A0A0U

#define TIMER_BASE           0x01C20C00U
#define TIMER_IRQ_EN         (TIMER_BASE + 0x00U)
#define TIMER_IRQ_STA        (TIMER_BASE + 0x04U)
#define TIMER0_CTRL          (TIMER_BASE + 0x10U)
#define TIMER0_INTERVAL      (TIMER_BASE + 0x14U)
#define TIMER0_IRQ_BIT       (1U << 0)
#define TIMER_CTRL_ENABLE    (1U << 0)
#define TIMER_CTRL_RELOAD    (1U << 1)
#define TIMER_CTRL_OSC24M    (1U << 2)
#define TIMER0_TICK_RELOAD   24000U
#define STOCK_TICK_COUNTER   0x42E68CD8U

static inline uint32_t mmio_read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static inline void mmio_write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

void irq_register_handler(uint32_t irq, uintptr_t handler,
                          uint32_t priority, uint32_t enable)
{
    volatile uint32_t *handlers =
        (volatile uint32_t *)(uintptr_t)STOCK_HANDLER_TABLE;
    uint32_t word_irq = irq & ~3U;
    uint32_t shift = (irq & 3U) * 8U;
    uint32_t priority_reg = GICD_IPRIORITYR + word_irq;
    uint32_t value;
    uint32_t bank_irq = irq & ~31U;
    uint32_t bit = irq - bank_irq;

    value = mmio_read32(priority_reg);
    value &= ~(0xFFU << shift);
    value |= ((priority << 3U) & 0xFFU) << shift;
    mmio_write32(priority_reg, value);

    handlers[irq] = (uint32_t)handler;

    if (enable != 0U)
        mmio_write32(GICD_ISENABLER + (bank_irq / 8U), 1U << bit);
    else
        mmio_write32(GICD_ICENABLER + (bank_irq / 8U), 1U << bit);
}

void timer0_tick_init(void)
{
    uint32_t value;

    mmio_write32(TIMER0_CTRL, 0U);
    mmio_write32(TIMER0_INTERVAL, TIMER0_TICK_RELOAD);

    value = mmio_read32(TIMER_IRQ_EN);
    mmio_write32(TIMER_IRQ_EN, value | TIMER0_IRQ_BIT);

    mmio_write32(TIMER0_CTRL, TIMER_CTRL_OSC24M | TIMER_CTRL_RELOAD);
    while ((mmio_read32(TIMER0_CTRL) & TIMER_CTRL_RELOAD) != 0U) {
        /* wait for reload */
    }

    mmio_write32(TIMER0_CTRL, TIMER_CTRL_OSC24M | TIMER_CTRL_ENABLE);
}

void timer0_tick_isr(void)
{
    volatile uint32_t *tick =
        (volatile uint32_t *)(uintptr_t)STOCK_TICK_COUNTER;

    /* Stock 0x40042C64 acknowledges timer0 by RMW-setting bit 0. */
    mmio_write32(TIMER_IRQ_STA, mmio_read32(TIMER_IRQ_STA) | TIMER0_IRQ_BIT);
    *tick = *tick + 1U;
}

void irq_enable_global(void)
{
    uint32_t cpsr;

    __asm__ volatile (
        "mrs %0, cpsr\n"
        "bic %0, %0, #0x80\n"
        "msr cpsr_c, %0\n"
        : "=&r" (cpsr)
        :
        : "memory", "cc");
}

void irq_controller_init(void)
{
    volatile uint32_t *state =
        (volatile uint32_t *)(uintptr_t)STOCK_IRQ_STATE;
    volatile uint32_t *handlers =
        (volatile uint32_t *)(uintptr_t)STOCK_HANDLER_TABLE;
    uint32_t irq_lines;
    uint32_t offset;
    uint32_t i;

    state[0] = GIC_BASE;
    state[1] = 0U;
    state[2] = 0U;
    state[3] = STOCK_IRQ_COUNT;

    for (i = 0U; i < STOCK_IRQ_COUNT; ++i)
        handlers[i] = STOCK_DEFAULT_ISR;

    mmio_write32(GICD_CTLR, 0U);

    irq_lines = ((mmio_read32(GICD_TYPER) & 0x1FU) + 1U) * 32U;
    if (irq_lines > 0x3FCU)
        irq_lines = 0x3FCU;

    for (offset = 32U; offset < irq_lines; offset += 32U)
        mmio_write32(GICD_ICENABLER + (offset / 8U), 0xFFFFFFFFU);

    for (offset = 32U; offset < irq_lines; offset += 4U)
        mmio_write32(GICD_IPRIORITYR + offset, STOCK_DEFAULT_PRIO);

    for (offset = 32U; offset < irq_lines; offset += 4U)
        mmio_write32(GICD_ITARGETSR + offset, 0x01010101U);

    for (offset = 32U; offset < irq_lines; offset += 16U)
        mmio_write32(GICD_ICFGR + (offset / 4U), 0U);

    mmio_write32(GICD_CTLR, 1U);

    mmio_write32(GICD_ICENABLER + 0U, 0xFFFF0000U);
    mmio_write32(GICD_ISENABLER + 0U, 0x0000FFFFU);

    for (offset = 0U; offset < 32U; offset += 4U)
        mmio_write32(GICD_IPRIORITYR + offset, STOCK_DEFAULT_PRIO);

    mmio_write32(GICC_PMR, 0xF8U);
    mmio_write32(GICC_CTLR, 1U);
}
