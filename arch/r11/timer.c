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

static uint32_t timer_start_ticks;

void timer_init(void)
{
    uint32_t ctrl;

    /* Stop Timer 0 before changing its interval and clock source. */
    mmio_write32(TIMER0_CTRL, 0U);
    mmio_write32(TIMER0_INTERVAL, 0xFFFFFFFFU);

    ctrl = TIMER_CTRL_OSC24M | TIMER_CTRL_PRESCALE_1 |
           TIMER_CTRL_RELOAD;
    mmio_write32(TIMER0_CTRL, ctrl);

    /* RELOAD is self-clearing; enabling afterwards starts the down-counter. */
    while (mmio_read32(TIMER0_CTRL) & TIMER_CTRL_RELOAD) {
        /* wait */
    }

    mmio_write32(TIMER0_CTRL,
                 TIMER_CTRL_OSC24M | TIMER_CTRL_PRESCALE_1 |
                 TIMER_CTRL_ENABLE);

    timer_start_ticks = mmio_read32(TIMER0_CURRENT);
}

uint32_t timer_ticks(void)
{
    /* Unsigned subtraction naturally handles one 32-bit down-counter wrap. */
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

    while (!timer_deadline_reached(deadline)) {
        __asm__ volatile ("nop");
    }
}
