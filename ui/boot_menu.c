#include <stdint.h>

#include "boot_menu.h"
#include "display.h"
#include "console.h"
#include "timer.h"
#include "gt911.h"
#include "stage2_update.h"
#include "stage2_version.h"
#include "watchdog.h"


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


#define TOUCH_POLL_INTERVAL_MS 10U

/* Shared BOOT/TOOLS menu geometry at console scale 2. */
#define MENU_X_MIN             16U
#define MENU_X_MAX             527U
#define MENU_FIRST_ITEM_ROW    4U
#define MENU_FIRST_ITEM_Y      96U
#define MENU_ITEM_STEP_Y       64U
#define MENU_ITEM_TOUCH_HEIGHT 64U

#define UPDATE_CONFIRM_X_MIN      16U
#define UPDATE_CONFIRM_X_MAX      527U
#define UPDATE_CANCEL_ROW         8U
#define UPDATE_INSTALL_ROW        12U
#define UPDATE_CANCEL_Y           (16U + UPDATE_CANCEL_ROW * 20U)
#define UPDATE_INSTALL_Y          (16U + UPDATE_INSTALL_ROW * 20U)
#define UPDATE_CONFIRM_HEIGHT     64U

typedef enum {
    MENU_PAGE_BOOT = 0,
    MENU_PAGE_TOOLS
} menu_page_t;

typedef enum {
    MENU_ACTION_NONE = 0,
    MENU_ACTION_BOOT_DEBUG,
    MENU_ACTION_BOOT_CUSTOM,
    MENU_ACTION_BOOT_SCOPE,
    MENU_ACTION_BOOT_USB,
    MENU_ACTION_OPEN_TOOLS,
    MENU_ACTION_UPDATE_STAGE2,
    MENU_ACTION_STOCK_UPDATER,
    MENU_ACTION_ENTER_FEL,
    MENU_ACTION_BACK
} menu_action_t;

typedef enum {
    TOOLS_MESSAGE_NONE = 0,
    TOOLS_MESSAGE_UPDATE_NOT_IMPLEMENTED,
    TOOLS_MESSAGE_FEL_NOT_IMPLEMENTED
} tools_message_t;


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


static void draw_menu_item(const char *title,
                           const char *status_text,
                           uint16_t status_color,
                           int active)
{
    if (active) {
        console_highlight_block_begin(DISPLAY_COLOR_CYAN);
        console_set_color(DISPLAY_COLOR_BLACK);
        console_putln(title);
        console_highlight_block_end();
    } else {
        console_set_color(DISPLAY_COLOR_WHITE);
        console_putln(title);
    }

    console_set_color(status_color);
    console_putln(status_text);
    console_putln("");
}


static void draw_target(const boot_context_t *context,
                        boot_target_id_t id,
                        int active)
{
    const boot_target_status_t *status = &context->status[id];

    if (status->available) {
        draw_menu_item(boot_target_name(id),
                       "    READY",
                       DISPLAY_COLOR_GREEN,
                       active);
    } else {
        /* Keep unavailable rows geometrically identical to available rows. */
        draw_menu_item(boot_target_name(id),
                       "    UNAVAILABLE",
                       DISPLAY_COLOR_GRAY,
                       active);
    }
}


static void draw_tools_entry(menu_action_t armed_action)
{
    draw_menu_item("TOOLS >",
                   "",
                   DISPLAY_COLOR_WHITE,
                   armed_action == MENU_ACTION_OPEN_TOOLS);
}

void boot_menu_show_usb_linux(void)
{
    console_clear();

    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("                        FNIRSI DPOS350P");
    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("                       USB MASS STORAGE");
    console_putln("");
    console_putln("");
    console_putln("");
    console_putln("");
    console_putln("");

    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("    +----------------------+          +------------------+");
    console_putln("    |                      |          |                  |");
    console_putln("    |     /\\       /\\      |   USB    |      LINUX       |");
    console_putln("    |____/  \\_____/  \\_____|=========>|       HOST       |");
    console_putln("    |                      |          |                  |");
    console_putln("    +----------------------+          +------------------+");
    console_putln("");

    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("             DPOS350P                  MASS STORAGE");
    console_putln("");
    console_putln("");
    console_putln("");

    console_set_color(DISPLAY_COLOR_GREEN);
    console_putln("                       LINUX MODE ACTIVE");
    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("                     READ / WRITE ENABLED");
    console_putln("");
    console_putln("");
    console_putln("");
    console_putln("");

    console_set_color(DISPLAY_COLOR_YELLOW);
    console_putln("                                  +----------------------+ ");
    console_putln("                                  |    HOLD TO REBOOT    | ");
    console_putln("                                  +----------------------+ ");

    console_present();
}


static void boot_menu_draw(const boot_context_t *context,
                           boot_target_id_t selected,
                           menu_action_t armed_action,
                           uint32_t seconds_left,
                           int touch_available,
                           int autoboot_enabled,
                           uint16_t touch_x,
                           uint16_t touch_y,
                           int have_touch)
{
    console_clear();

    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("FNIRSI DPOS350P");
    console_putln("BOOT MANAGER");

    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("--------------------------------");

    /* Overlay version without changing subsequent layout. */
    console_set_cursor(48U, 0U);
    console_puts("STG2 v");
    put_hex32(STAGE2_IMAGE_VERSION);

    /* Menu renderer already explicitly relocates itself. */
    console_set_cursor(1U, MENU_FIRST_ITEM_ROW);

    draw_target(
        context,
        BOOT_TARGET_DEBUG_EXE2_FAT,
        (autoboot_enabled && selected == BOOT_TARGET_DEBUG_EXE2_FAT) ||
        armed_action == MENU_ACTION_BOOT_DEBUG
    );

    draw_target(
        context,
        BOOT_TARGET_CUSTOM,
        (autoboot_enabled && selected == BOOT_TARGET_CUSTOM) ||
        armed_action == MENU_ACTION_BOOT_CUSTOM
    );

    draw_target(
        context,
        BOOT_TARGET_ORIGINAL_SCOPE,
        (autoboot_enabled && selected == BOOT_TARGET_ORIGINAL_SCOPE) ||
        armed_action == MENU_ACTION_BOOT_SCOPE
    );

    draw_target(
        context,
        BOOT_TARGET_USB_CONNECT_LINUX,
        (autoboot_enabled && selected == BOOT_TARGET_USB_CONNECT_LINUX) ||
        armed_action == MENU_ACTION_BOOT_USB
    );

    draw_tools_entry(armed_action);

    if (selected == BOOT_TARGET_NONE) {
        console_set_color(DISPLAY_COLOR_RED);
        console_putln("NO BOOTABLE TARGET FOUND");
    } else if (autoboot_enabled) {
        console_set_color(DISPLAY_COLOR_WHITE);
        console_puts("AUTOBOOT IN ");

        console_set_color(DISPLAY_COLOR_YELLOW);
        put_u32(seconds_left);

        console_set_color(DISPLAY_COLOR_WHITE);
        console_putln(" SECONDS");
    } else {
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("SELECT BOOT TARGET");
    }

#if BOOT_MENU_TOUCH_DIAGNOSTIC

    console_set_color(touch_available ? DISPLAY_COLOR_GREEN : DISPLAY_COLOR_RED);

    console_putln(
        touch_available
            ? "TOUCH DIAGNOSTIC ACTIVE"
            : "TOUCH INITIALIZATION FAILED"
    );

#else

    console_set_color(touch_available ? DISPLAY_COLOR_GREEN : DISPLAY_COLOR_GRAY);

    console_putln(
        touch_available
            ? "TOUCH ENABLED"
            : "TOUCH UNAVAILABLE - AUTOBOOT"
    );

#endif

    if (!autoboot_enabled && have_touch) {
        console_set_color(DISPLAY_COLOR_WHITE);
        console_puts("TOUCH X/Y: ");

        console_set_color(DISPLAY_COLOR_CYAN);
        put_u32(touch_x);
        console_puts(" / ");
        put_u32(touch_y);
        console_putln("");
    }

    console_present_swap_only();
}


static void update_status_line(const char *label, int ok)
{
    console_set_color(DISPLAY_COLOR_WHITE);
    console_puts(label);
    console_set_color(ok ? DISPLAY_COLOR_GREEN : DISPLAY_COLOR_RED);
    console_putln(ok ? "PASS" : "FAIL");
}


static void update_hex_line(const char *label, uint32_t value)
{
    console_set_color(DISPLAY_COLOR_WHITE);
    console_puts(label);
    put_hex32(value);
    console_putln("");
}


static const char *stage2_write_status_name(stage2_write_status_t status)
{
    switch (status) {
    case STAGE2_WRITE_OK:
        return "UPDATE SUCCESS";
    case STAGE2_WRITE_NOT_ARMED:
        return "WRITE NOT ARMED";
    case STAGE2_WRITE_INPUT_INVALID:
        return "INPUT INVALID";
    case STAGE2_WRITE_BACKUP_FAILED:
        return "BACKUP FAILED";
    case STAGE2_WRITE_INIT_FAILED:
        return "WRITE INIT FAILED";
    case STAGE2_WRITE_ERASE_FAILED:
        return "ERASE FAILED";
    case STAGE2_WRITE_PROGRAM_FAILED:
        return "PROGRAM FAILED";
    case STAGE2_WRITE_VERIFY_FAILED:
        return "VERIFY FAILED";
    case STAGE2_WRITE_RESTORED:
        return "FAILED - OLD SLOT RESTORED";
    case STAGE2_WRITE_RESTORE_FAILED:
        return "RESTORE FAILED - USE FEL";
    default:
        return "UNKNOWN WRITE STATUS";
    }
}


static void stage2_update_wait_for_tap(void)
{
    gt911_touch_t touch;
    uint32_t next_poll = timer_get_ms();
    int pressed = 0;

    for (;;) {
        uint32_t now = timer_get_ms();
        int result;

        if ((int32_t)(now - next_poll) < 0)
            continue;
        next_poll = now + TOUCH_POLL_INTERVAL_MS;
        result = gt911_poll(&touch);
        if (result < 0)
            return;
        if (result == 0)
            continue;
        if (touch.state == GT911_TOUCH_PRESS)
            pressed = 1;
        else if (pressed && touch.state == GT911_TOUCH_RELEASE)
            return;
    }
}


static const char *stage2_update_policy_name(stage2_update_policy_t policy)
{
    switch (policy) {
    case STAGE2_UPDATE_POLICY_ALREADY_CURRENT:
        return "ALREADY UP TO DATE";
    case STAGE2_UPDATE_POLICY_UPGRADE:
        return "UPDATE AVAILABLE";
    case STAGE2_UPDATE_POLICY_RECOVERY:
        return "RECOVERY UPDATE";
    case STAGE2_UPDATE_POLICY_DOWNGRADE_BLOCKED:
        return "DOWNGRADE BLOCKED";
    case STAGE2_UPDATE_POLICY_SAME_VERSION_MISMATCH:
        return "SAME VERSION MISMATCH";
    default:
        return "UPDATE POLICY INVALID";
    }
}

static int stage2_update_confirm_gui(uint32_t installed_version,
                                     uint32_t update_version)
{
    gt911_touch_t touch;
    int armed = 0; /* 0 none, 1 cancel, 2 install */
    uint32_t next_poll;

    console_clear();
    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("INSTALL STAGE 2 UPDATE?");
    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("--------------------------------");
    console_puts("INSTALLED VERSION ");
    put_hex32(installed_version);
    console_putln("");
    console_puts("UPDATE VERSION    ");
    put_hex32(update_version);
    console_putln("");
    console_putln("");
    console_set_color(DISPLAY_COLOR_RED);
    console_putln("DO NOT REMOVE POWER DURING UPDATE");

    /* Render and hit-test use exactly the same fixed geometry. */
    console_set_cursor(1U, UPDATE_CANCEL_ROW);
    console_set_color(DISPLAY_COLOR_YELLOW);
    console_putln("CANCEL");

    console_set_cursor(1U, UPDATE_INSTALL_ROW);
    console_set_color(DISPLAY_COLOR_GREEN);
    console_putln("INSTALL UPDATE");
    console_present_swap_only();

    next_poll = timer_get_ms();
    for (;;) {
        uint32_t now = timer_get_ms();
        int rc;
        int hit = 0;

        if ((int32_t)(now - next_poll) < 0)
            continue;
        next_poll = now + TOUCH_POLL_INTERVAL_MS;
        rc = gt911_poll(&touch);
        if (rc < 0)
            return 0;
        if (rc == 0)
            continue;

        if (touch.x >= UPDATE_CONFIRM_X_MIN &&
            touch.x <= UPDATE_CONFIRM_X_MAX) {
            if (touch.y >= UPDATE_CANCEL_Y &&
                touch.y < UPDATE_CANCEL_Y + UPDATE_CONFIRM_HEIGHT)
                hit = 1;
            else if (touch.y >= UPDATE_INSTALL_Y &&
                     touch.y < UPDATE_INSTALL_Y + UPDATE_CONFIRM_HEIGHT)
                hit = 2;
        }

        if (touch.state == GT911_TOUCH_PRESS) {
            armed = hit;
        } else if (touch.state == GT911_TOUCH_DRAG) {
            armed = hit;
        } else if (touch.state == GT911_TOUCH_RELEASE) {
            if (armed != 0 && hit == armed)
                return armed == 2;
            armed = 0;
        }
    }
}

static void stage2_update_show(void)
{
    static stage2_update_info_t info;
    static stage2_write_result_t write_result;
    stage2_write_status_t write_status;
    int rc;

    console_clear();
    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("STAGE 2 UPDATE");
    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("CHECK / WRITE / VERIFY");
    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("--------------------------------");
    console_putln("ANALYZING...");
    console_present_swap_only();

    rc = stage2_update_analyze(&info);

    console_clear();
    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("STAGE 2 UPDATE");
    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("CHECK / WRITE / VERIFY");
    console_putln("--------------------------------");
    update_status_line("SD-NAND          ", info.sd_result == 0);
    update_status_line("FAT              ", info.fat_result == 0);
    update_status_line("STAGE2.BIN       ", info.file_found);
    update_status_line("IMAGE HEADER     ", info.header_valid);
    update_status_line("IMAGE CRC32      ", info.input_valid);
    update_status_line("CURRENT SLOT     ", info.slot_valid);
    update_hex_line("INSTALLED VERSION", info.installed_image_version);
    update_hex_line("UPDATE VERSION   ", info.image_version);
    update_hex_line("UPDATE POLICY    ", (uint32_t)info.policy);
    update_status_line("EXACT MATCH      ", info.exact_match);
    update_hex_line("DIFFERENCES      ", info.differences);
    update_hex_line("FIRST DIFFERENCE ", info.first_difference);

    if (rc != 0 || !info.input_valid) {
        console_set_color(DISPLAY_COLOR_RED);
        console_putln("ANALYSIS FAILED - NO WRITE");
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("TAP TO RETURN");
        console_present_swap_only();
        stage2_update_wait_for_tap();
        return;
    }

    console_set_color(
        info.policy == STAGE2_UPDATE_POLICY_UPGRADE ||
        info.policy == STAGE2_UPDATE_POLICY_RECOVERY ||
        info.policy == STAGE2_UPDATE_POLICY_ALREADY_CURRENT
            ? DISPLAY_COLOR_GREEN
            : DISPLAY_COLOR_RED);
    console_putln(stage2_update_policy_name(info.policy));

    if (info.policy == STAGE2_UPDATE_POLICY_ALREADY_CURRENT ||
        info.policy == STAGE2_UPDATE_POLICY_DOWNGRADE_BLOCKED ||
        info.policy == STAGE2_UPDATE_POLICY_SAME_VERSION_MISMATCH ||
        info.policy == STAGE2_UPDATE_POLICY_INVALID) {
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("TAP TO RETURN");
        console_present_swap_only();
        stage2_update_wait_for_tap();
        return;
    }

    if (!stage2_update_confirm_gui(info.installed_image_version,
                                   info.image_version)) {
        console_clear();
        console_set_color(DISPLAY_COLOR_CYAN);
        console_putln("STAGE 2 UPDATE");
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("UPDATE CANCELLED");
        console_set_color(DISPLAY_COLOR_WHITE);
        console_putln("NO NOR CHANGES MADE");
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("TAP TO RETURN");
        console_present_swap_only();
        stage2_update_wait_for_tap();
        return;
    }

    console_clear();
    console_set_color(DISPLAY_COLOR_RED);
    console_putln("UPDATING STAGE 2");
    console_putln("DO NOT REMOVE POWER");
    console_present_swap_only();

    write_status = stage2_update_write(&write_result);

    console_clear();
    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("STAGE 2 UPDATE RESULT");
    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("--------------------------------");
    update_hex_line("BACKUP RC        ",
                    (uint32_t)write_result.backup_result);
    update_hex_line("WRITE INIT RC    ",
                    (uint32_t)write_result.write_init_result);
    update_hex_line("ERASE RC         ",
                    (uint32_t)write_result.erase_result);
    update_hex_line("PROGRAM RC       ",
                    (uint32_t)write_result.program_result);
    update_hex_line("VERIFY RC        ",
                    (uint32_t)write_result.verify_result);
    update_hex_line("RESTORE RC       ",
                    (uint32_t)write_result.restore_result);
    update_hex_line("FIRST DIFFERENCE ",
                    write_result.first_difference);

    console_putln("");
    console_set_color(write_status == STAGE2_WRITE_OK
                      ? DISPLAY_COLOR_GREEN
                      : DISPLAY_COLOR_RED);
    console_putln(stage2_write_status_name(write_status));

    if (write_status == STAGE2_WRITE_OK) {
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("REBOOTING...");
        console_present_swap_only();
        timer_delay_ms(1000U);
        watchdog_reboot();
    }

    /* Failures stay visible and never reboot automatically. */
    console_set_color(DISPLAY_COLOR_YELLOW);
    console_putln("TAP TO RETURN");
    console_present_swap_only();
    stage2_update_wait_for_tap();
}

static void tools_menu_draw(const boot_context_t *context,
                            menu_action_t armed_action,
                            tools_message_t message,
                            uint16_t touch_x,
                            uint16_t touch_y,
                            int have_touch)
{
    const boot_target_status_t *stock_updater;

    stock_updater =
        &context->status[BOOT_TARGET_USB_CONNECT_LEGACY];

    console_clear();

    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("FNIRSI DPOS350P");
    console_putln("TOOLS");

    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("--------------------------------");
    console_putln("");
    console_set_cursor(1U, MENU_FIRST_ITEM_ROW);

    draw_menu_item("UPDATE STAGE 2",
                   "    READY",
                   DISPLAY_COLOR_GREEN,
                   armed_action == MENU_ACTION_UPDATE_STAGE2);

    draw_menu_item("STOCK UPDATER",
                   stock_updater->available
                       ? "    READY"
                       : "    UNAVAILABLE",
                   stock_updater->available ? DISPLAY_COLOR_GREEN : DISPLAY_COLOR_GRAY,
                   armed_action == MENU_ACTION_STOCK_UPDATER);

    draw_menu_item("ENTER FEL",
                   "    NOT IMPLEMENTED",
                   DISPLAY_COLOR_GRAY,
                   armed_action == MENU_ACTION_ENTER_FEL);

    draw_menu_item("< BACK",
                   "",
                   DISPLAY_COLOR_WHITE,
                   armed_action == MENU_ACTION_BACK);

    if (message == TOOLS_MESSAGE_UPDATE_NOT_IMPLEMENTED) {
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("STAGE 2 UPDATE: NOT IMPLEMENTED");
    } else if (message == TOOLS_MESSAGE_FEL_NOT_IMPLEMENTED) {
        console_set_color(DISPLAY_COLOR_YELLOW);
        console_putln("FEL ENTRY: NOT IMPLEMENTED");
    } else {
        console_set_color(DISPLAY_COLOR_WHITE);
        console_putln("SELECT TOOL");
    }

    if (have_touch) {
        console_set_color(DISPLAY_COLOR_WHITE);
        console_puts("TOUCH X/Y: ");
        console_set_color(DISPLAY_COLOR_CYAN);
        put_u32(touch_x);
        console_puts(" / ");
        put_u32(touch_y);
        console_putln("");
    }

    console_present_swap_only();
}

static int menu_item_index(uint16_t x, uint16_t y,
                           unsigned item_count)
{
    uint32_t relative_y;
    unsigned index;

    if (x < MENU_X_MIN || x > MENU_X_MAX || y < MENU_FIRST_ITEM_Y)
        return -1;

    relative_y = (uint32_t)y - MENU_FIRST_ITEM_Y;
    index = (unsigned)(relative_y / MENU_ITEM_STEP_Y);

    if (index >= item_count ||
        (relative_y % MENU_ITEM_STEP_Y) >= MENU_ITEM_TOUCH_HEIGHT)
        return -1;

    return (int)index;
}

static menu_action_t boot_menu_hit_test(uint16_t x,
                                        uint16_t y)
{
    static const menu_action_t actions[] = {
        MENU_ACTION_BOOT_DEBUG,
        MENU_ACTION_BOOT_CUSTOM,
        MENU_ACTION_BOOT_SCOPE,
        MENU_ACTION_BOOT_USB,
        MENU_ACTION_OPEN_TOOLS
    };
    int index = menu_item_index(x, y,
                                (unsigned)(sizeof(actions) /
                                           sizeof(actions[0])));

    if (index < 0)
        return MENU_ACTION_NONE;


    return actions[index];
}

static menu_action_t tools_menu_hit_test(uint16_t x,
                                         uint16_t y)
{
    static const menu_action_t actions[] = {
        MENU_ACTION_UPDATE_STAGE2,
        MENU_ACTION_STOCK_UPDATER,
        MENU_ACTION_ENTER_FEL,
        MENU_ACTION_BACK
    };
    int index = menu_item_index(x, y,
                                (unsigned)(sizeof(actions) /
                                           sizeof(actions[0])));

    if (index < 0)
        return MENU_ACTION_NONE;


    return actions[index];
}

static boot_target_id_t action_boot_target(menu_action_t action)
{
    switch (action) {
    case MENU_ACTION_BOOT_DEBUG:
        return BOOT_TARGET_DEBUG_EXE2_FAT;
    case MENU_ACTION_BOOT_CUSTOM:
        return BOOT_TARGET_CUSTOM;
    case MENU_ACTION_BOOT_SCOPE:
        return BOOT_TARGET_ORIGINAL_SCOPE;
    case MENU_ACTION_BOOT_USB:
        return BOOT_TARGET_USB_CONNECT_LINUX;
    case MENU_ACTION_STOCK_UPDATER:
        return BOOT_TARGET_USB_CONNECT_LEGACY;
    default:
        return BOOT_TARGET_NONE;
    }
}


boot_target_id_t boot_menu_run(const boot_context_t *context,
                               int touch_available)
{
    boot_target_id_t selected;
    boot_target_id_t hit_target;
    menu_page_t page;
    menu_action_t action;
    menu_action_t armed_action;
    menu_page_t armed_page;
    tools_message_t tools_message;

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
    page = MENU_PAGE_BOOT;
    armed_page = MENU_PAGE_BOOT;
    armed_action = MENU_ACTION_NONE;
    tools_message = TOOLS_MESSAGE_NONE;

    shown = 0xFFFFFFFFU;
    seconds_left = BOOT_MENU_AUTOBOOT_SECONDS;

    last_touch_x = 0U;
    last_touch_y = 0U;
    have_touch = 0;

    if (selected == BOOT_TARGET_NONE) {
        boot_menu_draw(
            context,
            selected,
            armed_action,
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
        armed_action,
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
        armed_action,
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
                    armed_action,
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

            if (page == MENU_PAGE_TOOLS) {
                tools_menu_draw(
                    context,
                    armed_action,
                    tools_message,
                    last_touch_x,
                    last_touch_y,
                    have_touch
                );
            } else {
                boot_menu_draw(
                    context,
                    selected,
                    armed_action,
                    autoboot_enabled ? seconds_left : 0U,
                    0,
                    autoboot_enabled,
                    last_touch_x,
                    last_touch_y,
                    have_touch
                );
            }

            continue;
        }

        if (touch_result == 0)
            continue;

        /*
         * Stage 2 uses normal release-to-activate button semantics:
         * PRESS arms/highlights the item, DRAG may cancel it, and RELEASE
         * activates only when it occurs over the same item.
         */
        if (touch.state == GT911_TOUCH_PRESS) {
            autoboot_enabled = 0;
            last_touch_x = touch.x;
            last_touch_y = touch.y;
            have_touch = 1;
            armed_page = page;
            armed_action = page == MENU_PAGE_BOOT
                         ? boot_menu_hit_test(touch.x, touch.y)
                         : tools_menu_hit_test(touch.x, touch.y);

            /* Highlight the armed boot target when applicable. */
            if (page == MENU_PAGE_BOOT) {
                boot_menu_draw(context, selected, armed_action, 0U, touch_available, 0,
                               last_touch_x, last_touch_y, have_touch);
            } else {
                tools_menu_draw(context, armed_action, tools_message, last_touch_x,
                                last_touch_y, have_touch);
            }
            continue;
        }

        if (touch.state == GT911_TOUCH_HELD)
            continue;

        if (touch.state == GT911_TOUCH_DRAG) {
            menu_action_t new_action;

            /*
             * Drag-to-select: while the finger remains down, move the armed
             * selection to whichever item is currently under the finger.
             * Redraw only when the hit-tested action actually changes.
             */
            new_action = page == MENU_PAGE_BOOT
                       ? boot_menu_hit_test(touch.x, touch.y)
                       : tools_menu_hit_test(touch.x, touch.y);

            if (page != armed_page)
                new_action = MENU_ACTION_NONE;

            if (new_action != armed_action) {
                armed_action = new_action;
                last_touch_x = touch.x;
                last_touch_y = touch.y;
                have_touch = 1;

                if (page == MENU_PAGE_BOOT)
                    boot_menu_draw(context, selected, armed_action, 0U,
                                   touch_available, 0, last_touch_x,
                                   last_touch_y, have_touch);
                else
                    tools_menu_draw(context, armed_action, tools_message,
                                    last_touch_x, last_touch_y, have_touch);
            }
            continue;
        }

        if (touch.state != GT911_TOUCH_RELEASE)
            continue;

        last_touch_x = touch.x;
        last_touch_y = touch.y;
        have_touch = 1;

        action = page == MENU_PAGE_BOOT
               ? boot_menu_hit_test(touch.x, touch.y)
               : tools_menu_hit_test(touch.x, touch.y);

        if (page != armed_page ||
            armed_action == MENU_ACTION_NONE ||
            action != armed_action) {
            armed_action = MENU_ACTION_NONE;
            if (page == MENU_PAGE_BOOT)
                boot_menu_draw(context, selected, armed_action, 0U, touch_available, 0,
                               last_touch_x, last_touch_y, have_touch);
            else
                tools_menu_draw(context, armed_action, tools_message, last_touch_x,
                                last_touch_y, have_touch);
            continue;
        }

        action = armed_action;
        armed_action = MENU_ACTION_NONE;

        if (page == MENU_PAGE_BOOT) {
            if (action == MENU_ACTION_OPEN_TOOLS) {
                page = MENU_PAGE_TOOLS;
                tools_message = TOOLS_MESSAGE_NONE;

                tools_menu_draw(
                    context,
                    armed_action,
                    tools_message,
                    last_touch_x,
                    last_touch_y,
                    have_touch
                );

                continue;
            }

            hit_target = action_boot_target(action);

            if (hit_target != BOOT_TARGET_NONE &&
                context->status[hit_target].available) {

#if BOOT_MENU_TOUCH_DIAGNOSTIC
                boot_menu_draw(
                    context,
                    selected,
                    armed_action,
                    0U,
                    touch_available,
                    0,
                    last_touch_x,
                    last_touch_y,
                    have_touch
                );

                continue;
#else
                return hit_target;
#endif
            }

            boot_menu_draw(
                context,
                selected,
                armed_action,
                0U,
                touch_available,
                0,
                last_touch_x,
                last_touch_y,
                have_touch
            );

            continue;
        }

        if (action == MENU_ACTION_BACK) {
            page = MENU_PAGE_BOOT;
            tools_message = TOOLS_MESSAGE_NONE;

            boot_menu_draw(
                context,
                selected,
                armed_action,
                0U,
                touch_available,
                0,
                last_touch_x,
                last_touch_y,
                have_touch
            );

            continue;
        }

        if (action == MENU_ACTION_UPDATE_STAGE2) {
            stage2_update_show();
            tools_message = TOOLS_MESSAGE_NONE;
            armed_action = MENU_ACTION_NONE;

            tools_menu_draw(
                context,
                armed_action,
                tools_message,
                last_touch_x,
                last_touch_y,
                have_touch
            );

            continue;
        }

        if (action == MENU_ACTION_ENTER_FEL) {
            tools_message = TOOLS_MESSAGE_FEL_NOT_IMPLEMENTED;

            tools_menu_draw(
                context,
                armed_action,
                tools_message,
                last_touch_x,
                last_touch_y,
                have_touch
            );

            continue;
        }

        hit_target = action_boot_target(action);

        if (hit_target == BOOT_TARGET_USB_CONNECT_LEGACY &&
            context->status[hit_target].available) {
            return hit_target;
        }

        tools_menu_draw(
            context,
            armed_action,
            tools_message,
            last_touch_x,
            last_touch_y,
            have_touch
        );
    }
}


void boot_menu_show_loading(boot_target_id_t selected)
{
    console_clear();

    console_set_color(DISPLAY_COLOR_CYAN);
    console_putln("FNIRSI DPOS350P");
    console_putln("BOOT MANAGER");
    console_putln("");

    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("LOADING");
    console_putln("");

    console_set_color(DISPLAY_COLOR_YELLOW);
    console_putln(boot_target_name(selected));
    console_putln("");

    console_set_color(DISPLAY_COLOR_WHITE);
    console_putln("PLEASE WAIT...");

    console_present();
}


void boot_menu_show_error(boot_target_id_t selected,
                          int error)
{
    console_clear();

    console_set_color(DISPLAY_COLOR_RED);
    console_putln("BOOT FAILED");
    console_putln("");

    console_set_color(DISPLAY_COLOR_WHITE);
    console_puts("TARGET: ");
    console_putln(boot_target_name(selected));

    console_puts("ERROR : ");

    console_set_color(DISPLAY_COLOR_RED);
    put_hex32((uint32_t)error);
    console_putln("");

    console_present();
}
