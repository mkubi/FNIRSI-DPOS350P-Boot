#include <stdint.h>
#include "watchdog.h"

#define TIMER_BASE   0x01C20C00U
#define WDOG_CTRL    (TIMER_BASE + 0x0B0U)
#define WDOG_CFG     (TIMER_BASE + 0x0B4U)
#define WDOG_MODE    (TIMER_BASE + 0x0B8U)
#define WDOG_KEY     (0x0A57U << 1)
#define WDOG_RESTART (1U << 0)

static void write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

void watchdog_reboot(void)
{
    __asm__ volatile ("cpsid if" ::: "memory");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");

    write32(WDOG_CFG, 1U);
    write32(WDOG_MODE, 1U);
    write32(WDOG_CTRL, WDOG_KEY | WDOG_RESTART);

    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");

    for (;;)
        __asm__ volatile ("wfi");
}
