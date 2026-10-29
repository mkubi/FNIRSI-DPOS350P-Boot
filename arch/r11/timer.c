#include <stdint.h>
#include "timer.h"
#include "regs.h"

#define TIMER_BASE             0x01C20C00U
#define TIMER0_CTRL            (TIMER_BASE + 0x10U)
#define TIMER0_INTERVAL        (TIMER_BASE + 0x14U)
#define TIMER0_CURRENT         (TIMER_BASE + 0x18U)

#define TIMER_CTRL_ENABLE      (1U << 0)
#define TIMER_CTRL_RELOAD      (1U << 1)
#define TIMER_CTRL_OSC24M      (1U << 2)
#define TIMER_CTRL_PRESCALE_1  (0U << 4)

#define TIMER_CLOCK_HZ         24000000U
#define TIMER_TICKS_PER_MS     (TIMER_CLOCK_HZ / 1000U)

#define AVS_ENABLE_REG         0x01C20144U
#define AVS_DIVIDER            0x01C20C8CU
#define AVS_ENABLE_BIT         (1U << 31)
#define AVS_COUNTER0_DIVISOR   24000U
#define AVS_COUNTER1_DIVISOR   24U
#define AVS_CTRL               0x01C20C80U
#define AVS_COUNTER0           0x01C20C84U
#define AVS_CTRL_COUNTER0_EN   (1U << 0)

static uint32_t timer_start_ticks;

static void timer_avs_set_divisor(uint32_t channel, uint32_t divisor)
{
    uint32_t value;

    value = mmio_read32(AVS_ENABLE_REG);
    mmio_write32(AVS_ENABLE_REG, value | AVS_ENABLE_BIT);

    value = mmio_read32(AVS_DIVIDER);

    if (channel == 0U) {
        value &= 0xFFFF0000U;
        value |= divisor - 1U;
    } else if (channel == 1U) {
        value &= 0x0000FFFFU;
        value |= (divisor - 1U) << 16;
    }

    mmio_write32(AVS_DIVIDER, value);
}

static void timer_avs_init(void)
{
    timer_avs_set_divisor(0U, AVS_COUNTER0_DIVISOR);
    timer_avs_set_divisor(1U, AVS_COUNTER1_DIVISOR);
}

void timer_init(void)
{
    uint32_t ctrl;

    mmio_write32(TIMER0_CTRL, 0U);
    mmio_write32(TIMER0_INTERVAL, 0xFFFFFFFFU);

    ctrl = TIMER_CTRL_OSC24M | TIMER_CTRL_PRESCALE_1 |
           TIMER_CTRL_RELOAD;
    mmio_write32(TIMER0_CTRL, ctrl);

    while (mmio_read32(TIMER0_CTRL) & TIMER_CTRL_RELOAD) {
        /* wait */
    }

    mmio_write32(TIMER0_CTRL,
                 TIMER_CTRL_OSC24M | TIMER_CTRL_PRESCALE_1 |
                 TIMER_CTRL_ENABLE);

    timer_start_ticks = mmio_read32(TIMER0_CURRENT);

    /* Stock System_Init initializes both AVS divider channels. */
    timer_avs_init();
}

uint32_t timer_ticks(void)
{
    return timer_start_ticks - mmio_read32(TIMER0_CURRENT);
}

uint32_t timer_get_ms(void)
{
    return timer_ticks() / TIMER_TICKS_PER_MS;
}

void timer_delay_us(uint32_t microseconds)
{
    uint32_t start;
    uint32_t required_ticks;

    if (microseconds == 0U)
        return;

    start = timer_ticks();
    required_ticks = microseconds * 24U;

    while ((uint32_t)(timer_ticks() - start) < required_ticks)
        __asm__ volatile ("nop");
}

int timer_deadline_reached(uint32_t deadline_ms)
{
    return (int32_t)(timer_get_ms() - deadline_ms) >= 0;
}

void timer_delay_ms(uint32_t milliseconds)
{
    uint32_t deadline = timer_get_ms() + milliseconds;

    while (!timer_deadline_reached(deadline))
        __asm__ volatile ("nop");
}

void timer_avs_delay_units(uint32_t units)
{
    uint32_t control;

    if (units == 0U)
        return;

    mmio_write32(AVS_COUNTER0, 0U);

    control = mmio_read32(AVS_CTRL);
    mmio_write32(AVS_CTRL, control | AVS_CTRL_COUNTER0_EN);

    while ((mmio_read32(AVS_COUNTER0) << 1) < units)
        __asm__ volatile ("nop");

    __asm__ volatile ("dmb sy" ::: "memory");
}

