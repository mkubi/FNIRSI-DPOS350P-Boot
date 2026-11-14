#include <stdint.h>

#include "gt911.h"
#include "irq.h"
#include "touch_reboot.h"

/* Allwinner R11 timer block. */
#define TIMER_BASE                  0x01C20C00U
#define TIMER_IRQ_EN                (TIMER_BASE + 0x00U)
#define TIMER_IRQ_STATUS            (TIMER_BASE + 0x04U)
#define TIMER1_CTRL                 (TIMER_BASE + 0x20U)
#define TIMER1_INTERVAL             (TIMER_BASE + 0x24U)

/* Timer1 IRQ. */
#define TIMER1_IRQ                  51U
#define TIMER1_IRQ_BIT              (1U << 1)

/* Timer1 control. */
#define TIMER_CTRL_ENABLE           (1U << 0)
#define TIMER_CTRL_RELOAD           (1U << 1)
#define TIMER_CTRL_OSC24M           (1U << 2)

/* 24 MHz * 50 ms = 1,200,000 ticks. */
#define TIMER1_50MS_TICKS           1200000U

/* Reboot button hitbox. */
#define REBOOT_X_MIN                650U
#define REBOOT_X_MAX                1023U
#define REBOOT_Y_MIN                520U
#define REBOOT_Y_MAX                599U

/* Watchdog registers. */
#define WDOG_CTRL                   (TIMER_BASE + 0x0B0U)
#define WDOG_CFG                    (TIMER_BASE + 0x0B4U)
#define WDOG_MODE                   (TIMER_BASE + 0x0B8U)

#define WDOG_KEY                    (0x0A57U << 1)
#define WDOG_RESTART                (1U << 0)

static uint32_t read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static void watchdog_reboot(void)
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

static int reboot_hit(const gt911_touch_t *touch)
{
    return touch->x >= REBOOT_X_MIN &&
           touch->x <= REBOOT_X_MAX &&
           touch->y >= REBOOT_Y_MIN &&
           touch->y <= REBOOT_Y_MAX;
}

static void touch_reboot_isr(void)
{
    gt911_touch_t touch;

    /* Timer1 pending bit is write-one-to-clear. */
    write32(TIMER_IRQ_STATUS, TIMER1_IRQ_BIT);

    /*
     * gt911_poll():
     *   > 0  new DATA_READY sample
     *   = 0  no new sample
     *   < 0  error
     */
    if (gt911_poll(&touch) > 0 &&
        touch.pressed != 0U &&
        touch.points != 0U &&
        reboot_hit(&touch)) {
        watchdog_reboot();
    }
}

int touch_reboot_init(void)
{
    gt911_info_t info;
    uint32_t value;

    /*
     * Initialize GT911 before enabling periodic touch polling.
     */
    if (gt911_init() != 0)
        return -1;

    if (gt911_probe(&info) != 0)
        return -2;

    /*
     * Configure Timer1 while stopped and clear any stale pending IRQ.
     */
    write32(TIMER1_CTRL, 0U);
    write32(TIMER_IRQ_STATUS, TIMER1_IRQ_BIT);
    write32(TIMER1_INTERVAL, TIMER1_50MS_TICKS);

    /*
     * Register and enable the Timer1 interrupt.
     *
     * Keep the priority value identical to the HW-confirmed B2c build.
     */
    irq_register_handler(TIMER1_IRQ,
                         (uintptr_t)touch_reboot_isr,
                         0U,
                         1U);

    /*
     * Enable Timer1 IRQ without modifying other timer interrupt sources.
     */
    value = read32(TIMER_IRQ_EN);
    value |= TIMER1_IRQ_BIT;
    write32(TIMER_IRQ_EN, value);

    /*
     * Start Timer1 in periodic reload mode from OSC24M.
     */
    write32(TIMER1_CTRL,
            TIMER_CTRL_OSC24M |
            TIMER_CTRL_RELOAD |
            TIMER_CTRL_ENABLE);

    /*
     * Return immediately so USB Mass Storage initialization can continue.
     */
    return 0;
}
