#include <stdint.h>
#include "fel.h"
#include "spi0.h"

#define TIMER_BASE       0x01C20C00U
#define WDOG_CTRL        (TIMER_BASE + 0x0B0U)
#define WDOG_CFG         (TIMER_BASE + 0x0B4U)
#define WDOG_MODE        (TIMER_BASE + 0x0B8U)
#define WDOG_KEY         (0x0A57U << 1)
#define WDOG_RESTART     (1U << 0)
#define WDOG_CFG_SYSTEM  0x00000001U
#define WDOG_MODE_500MS  0x00000001U

static inline void write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static void settle(void)
{
    volatile uint32_t i;
    for (i = 0U; i < 100000U; ++i)
        __asm__ volatile ("nop");
}

void enter_fel_recovery(void)
{
    static const uint8_t command[1] = { 0xB9U };
    uint8_t response[1];

    /* HW-confirmed recovery primitive: put W25Q16JV into Deep Power-Down. */
    (void)spi0_transfer(command, response, 1U);
    settle();

    /* No further NOR accesses below this point. */
    __asm__ volatile ("cpsid if" ::: "memory");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    write32(WDOG_CFG, WDOG_CFG_SYSTEM);
    write32(WDOG_MODE, WDOG_MODE_500MS);
    write32(WDOG_CTRL, WDOG_KEY | WDOG_RESTART);
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    for (;;)
        __asm__ volatile ("wfi");
}
