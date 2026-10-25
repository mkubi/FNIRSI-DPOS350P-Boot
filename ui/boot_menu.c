#include <stdint.h>

#include "boot_menu.h"
#include "console.h"
#include "timer.h"
#include "gt911.h"


/*
 * 1 = touch diagnostic:
 *     - autoboot disabled,
 *     - touch only highlights a target,
 *     - no application is started.
 *
 * 0 = production behavior:
 *     - normal autoboot,
 *     - touching a target starts it immediately,
 *     - touching outside targets cancels autoboot.
 */
#define BOOT_MENU_TOUCH_DIAGNOSTIC 0

#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define CYAN   0x07FFU
#define GRAY   0x8410U

#define TOUCH_POLL_INTERVAL_MS 10U


static char hex_digit(uint8_t value)
{
    return value < 10U
        ? (char)('0' + value)
        : (char)('A' + value - 10U);
}


static void put_hex32(uint32_t value)
{
    char text[9];
    unsigned i;

    for (i = 0U; i < 8U; i++) {
        text[i] = hex_digit(
            (uint8_t)((value >> (28U - i * 4U)) & 15U)
        );
    }

    text[8] = 0;
    console_puts(text);
}


static void put_u32(uint32_t value)
{
    char text[11];
    unsigned pos = sizeof(text) - 1U;

    text[pos] = 0;

    do {
        text[--pos] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value && pos > 0U);

    console_puts(&text[pos]);
}


static void draw_target(const boot_context_t *context,
                        boot_target_id_t id,
                        boot_target_id_t selected)
{
    const boot_target_status_t *status = &context->status[id];

    console_putln("");

    console_set_color(id == selected ? YELLOW : WHITE);
    console_puts(id == selected ? ">>  " : "    ");
    console_putln(boot_target_name(id));

    console_puts("    ");

    if (status->available) {
        console_set_color(GREEN);
        console_putln("READY");
    } else {
        console_set_color(GRAY);
        console_puts("UNAVAILABLE  ");
        put_hex32((uint32_t)status->probe_result);
        console_putln("");
    }

    console_putln("");
}


static void boot_menu_draw(const boot_context_t *context,
                           boot_target_id_t selected,
                           uint32_t seconds_left,
                           int touch_available,
                           int autoboot_enabled,
                           uint16_t touch_x,
                           uint16_t touch_y,
                           int have_touch)
{
    console_clear();

    console_set_color(CYAN);
    console_putln("FNIRSI DPOS350P");
    console_putln("BOOT MANAGER");

    console_set_color(WHITE);
    console_putln("--------------------------------");

    draw_target(
        context,
        BOOT_TARGET_DEBUG_EXE2_FAT,
        selected
    );

    draw_target(
        context,
        BOOT_TARGET_CUSTOM,
        selected
    );

    draw_target(
        context,
        BOOT_TARGET_ORIGINAL_SCOPE,
        selected
    );

    draw_target(
        context,
        BOOT_TARGET_USB_CONNECT,
        selected
    );

    if (selected == BOOT_TARGET_NONE) {
        console_set_color(RED);
        console_putln("NO BOOTABLE TARGET FOUND");
    } else if (autoboot_enabled) {
        console_set_color(WHITE);
        console_puts("AUTOBOOT IN ");

        console_set_color(YELLOW);
        put_u32(seconds_left);

        console_set_color(WHITE);
        console_putln(" SECONDS");
    } else {
        console_set_color(YELLOW);
        console_putln("SELECT BOOT TARGET");
    }

#if BOOT_MENU_TOUCH_DIAGNOSTIC

    console_set_color(touch_available ? GREEN : RED);

    console_putln(
        touch_available
            ? "TOUCH DIAGNOSTIC ACTIVE"
            : "TOUCH INITIALIZATION FAILED"
    );

#else

    console_set_color(touch_available ? GREEN : GRAY);

    console_putln(
        touch_available
            ? "TOUCH ENABLED"
            : "TOUCH UNAVAILABLE - AUTOBOOT"
    );

#endif

    if (!autoboot_enabled && have_touch) {
        console_set_color(WHITE);
        console_puts("TOUCH X/Y: ");

        console_set_color(CYAN);
        put_u32(touch_x);
        console_puts(" / ");
        put_u32(touch_y);
        console_putln("");
    }

    console_present();
}


static boot_target_id_t boot_menu_hit_test(uint16_t x,
                                           uint16_t y)
{
    /*
     * Current confirmed console-menu regions.
     */
    if (x < 20U || x > 1003U)
        return BOOT_TARGET_NONE;

    if (y >= 48U && y < 130U)
        return BOOT_TARGET_DEBUG_EXE2_FAT;

    if (y >= 130U && y < 220U)
        return BOOT_TARGET_CUSTOM;

    if (y >= 220U && y < 310U)
        return BOOT_TARGET_ORIGINAL_SCOPE;

    if (y >= 310U && y < 400U)
        return BOOT_TARGET_USB_CONNECT;

    return BOOT_TARGET_NONE;
}


boot_target_id_t boot_menu_run(const boot_context_t *context,
                               int touch_available)
{
    boot_target_id_t selected;
    boot_target_id_t hit;

    gt911_touch_t touch;

    uint32_t deadline;
    uint32_t shown;
    uint32_t now;
    uint32_t remaining_ms;
    uint32_t seconds_left;
    uint32_t next_touch_poll;

    uint16_t last_touch_x;
    uint16_t last_touch_y;

    int touch_result;
    int have_touch;

#if BOOT_MENU_TOUCH_DIAGNOSTIC
    int autoboot_enabled = 0;
#else
    int autoboot_enabled = 1;
#endif

    selected = boot_targets_default(context);

    shown = 0xFFFFFFFFU;
    seconds_left = BOOT_MENU_AUTOBOOT_SECONDS;

    last_touch_x = 0U;
    last_touch_y = 0U;
    have_touch = 0;

    if (selected == BOOT_TARGET_NONE) {
        boot_menu_draw(
            context,
            selected,
            0U,
            touch_available,
            0,
            0U,
            0U,
            0
        );

        return BOOT_TARGET_NONE;
    }

    now = timer_get_ms();

    deadline = now
             + BOOT_MENU_AUTOBOOT_SECONDS * 1000U;

    next_touch_poll = now;

#if BOOT_MENU_TOUCH_DIAGNOSTIC

    /*
     * Diagnostic mode starts paused and never automatically exits.
     */
    boot_menu_draw(
        context,
        selected,
        0U,
        touch_available,
        0,
        last_touch_x,
        last_touch_y,
        have_touch
    );

#else

    boot_menu_draw(
        context,
        selected,
        seconds_left,
        touch_available,
        1,
        last_touch_x,
        last_touch_y,
        have_touch
    );

    shown = seconds_left;

#endif

    for (;;) {
        now = timer_get_ms();

        if (autoboot_enabled) {
            if (timer_deadline_reached(deadline))
                return selected;

            remaining_ms = deadline - now;
            seconds_left = (remaining_ms + 999U) / 1000U;

            if (seconds_left != shown) {
                boot_menu_draw(
                    context,
                    selected,
                    seconds_left,
                    touch_available,
                    1,
                    last_touch_x,
                    last_touch_y,
                    have_touch
                );

                shown = seconds_left;
            }
        }

        if (!touch_available)
            continue;

        /*
         * Poll the bit-banged I2C interface at most once per 10 ms.
         */
        if ((int32_t)(now - next_touch_poll) < 0)
            continue;

        next_touch_poll = now + TOUCH_POLL_INTERVAL_MS;

        touch_result = gt911_poll(&touch);

        if (touch_result < 0) {
            /*
             * Runtime touch communication failed.
             *
             * In production mode, disable touch but preserve autoboot
             * as the recovery path.
             *
             * In diagnostic mode, remain in the menu indefinitely.
             */
            touch_available = 0;

#if BOOT_MENU_TOUCH_DIAGNOSTIC
            autoboot_enabled = 0;
#endif

            boot_menu_draw(
                context,
                selected,
                autoboot_enabled ? seconds_left : 0U,
                0,
                autoboot_enabled,
                last_touch_x,
                last_touch_y,
                have_touch
            );

            continue;
        }

        if (touch_result == 0)
            continue;

        /*
         * Ignore release events and stale DATA_READY events that contain
         * no active point.
         *
         * These events must not:
         * - cancel autoboot,
         * - replace the last valid coordinates with 0/0,
         * - select a target.
         */
        if (!touch.pressed || touch.points == 0U)
            continue;

        /*
         * Only a real pressed point cancels autoboot.
         */
        autoboot_enabled = 0;

        /*
         * Store only valid pressed coordinates.
         */
        last_touch_x = touch.x;
        last_touch_y = touch.y;
        have_touch = 1;

        hit = boot_menu_hit_test(
            touch.x,
            touch.y
        );

        if (hit != BOOT_TARGET_NONE &&
            context->status[hit].available) {

#if BOOT_MENU_TOUCH_DIAGNOSTIC

            /*
             * Diagnostic mode only highlights the detected target.
             */
            selected = hit;

            boot_menu_draw(
                context,
                selected,
                0U,
                touch_available,
                0,
                last_touch_x,
                last_touch_y,
                have_touch
            );

            continue;

#else

            /*
             * Production mode immediately boots the touched target.
             */
            return hit;

#endif
        }

        /*
         * A real touch outside every valid target permanently cancels
         * autoboot and leaves the menu waiting for user selection.
         */
        boot_menu_draw(
            context,
            selected,
            0U,
            touch_available,
            0,
            last_touch_x,
            last_touch_y,
            have_touch
        );
    }
}


void boot_menu_show_loading(boot_target_id_t selected)
{
    console_clear();

    console_set_color(CYAN);
    console_putln("FNIRSI DPOS350P");
    console_putln("BOOT MANAGER");
    console_putln("");

    console_set_color(WHITE);
    console_putln("LOADING");
    console_putln("");

    console_set_color(YELLOW);
    console_putln(boot_target_name(selected));
    console_putln("");

    console_set_color(WHITE);
    console_putln("PLEASE WAIT...");

    console_present();
}


void boot_menu_show_error(boot_target_id_t selected,
                          int error)
{
    console_clear();

    console_set_color(RED);
    console_putln("BOOT FAILED");
    console_putln("");

    console_set_color(WHITE);
    console_puts("TARGET: ");
    console_putln(boot_target_name(selected));

    console_puts("ERROR : ");

    console_set_color(RED);
    put_hex32((uint32_t)error);
    console_putln("");

    console_present();
}
