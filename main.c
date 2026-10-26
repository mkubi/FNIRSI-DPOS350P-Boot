#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"
#include "gt911.h"
#include "boot_targets.h"
#include "boot_menu.h"
#include "start.h"
#include "dposdbg.h"
#include "dposlab.h"

#define BLACK 0x0000U
#define WHITE 0xFFFFU

static dposlab_handoff_t dposlab_handoff;

static char dposdbg_hex_digit(uint32_t value)
{
    return value < 10U ? (char)('0' + value) : (char)('A' + value - 10U);
}

static void dposdbg_put_hex32(uint32_t value)
{
    char text[11];
    unsigned int i;
    text[0] = '0';
    text[1] = 'x';
    for (i = 0U; i < 8U; ++i)
        text[2U + i] = dposdbg_hex_digit((value >> (28U - i * 4U)) & 0xFU);
    text[10] = '\0';
    console_puts(text);
}

static void dposdbg_put_u32(uint32_t value)
{
    char text[11];
    unsigned int pos = sizeof(text) - 1U;
    text[pos] = '\0';
    do {
        text[--pos] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    console_puts(&text[pos]);
}

static void dposdbg_show_diag(int result)
{
    const dposdbg_diag_t *diag = dposdbg_last_diag();
    console_set_color(WHITE);
    console_puts("DPOSDBG ABI: ");
    dposdbg_put_u32(diag->abi_version);
    console_puts("  HOOKS: ");
    dposdbg_put_u32(diag->hook_count);
    console_putln("");
    console_puts("PHASE: ");
    console_putln(dposdbg_diag_phase_name(diag->phase));
    console_puts("RESULT: ");
    dposdbg_put_hex32((uint32_t)result);
    console_putln("");
    console_puts("HOOK: ");
    dposdbg_put_u32(diag->hook_index);
    console_puts(" / ");
    dposdbg_put_u32(diag->hook_count);
    console_puts("  TYPE: ");
    console_putln(dposdbg_patch_type_name(diag->patch_type));
    console_puts("SITE: ");
    dposdbg_put_hex32(diag->site);
    console_puts("  VENEER: ");
    dposdbg_put_hex32(diag->veneer_address);
    console_putln("");
    console_puts("EXPECTED: ");
    dposdbg_put_hex32(diag->expected_word);
    console_puts("  ACTUAL: ");
    dposdbg_put_hex32(diag->actual_word);
    console_putln("");
    console_puts("TARGET: ");
    dposdbg_put_hex32(diag->target_address);
    console_puts("  GENERATED: ");
    dposdbg_put_hex32(diag->generated_word);
    console_putln("");
}

int main(void)
{
    static boot_context_t context;
    gt911_info_t touch_info;
    boot_target_id_t selected;
    const loader_image_t *image;
    int touch_available;
    int load_result;
    int dposdbg_result;
    loader_image_t dposlab_image;

    display_init();
    console_init(WHITE, BLACK, 2U);
    timer_init();

    console_set_color(WHITE);
    console_putln("PROBING BOOT TARGETS...");
    console_present();

    boot_targets_probe(&context);
    touch_available = gt911_init(&touch_info) == 0;
    selected = boot_menu_run(&context, touch_available);

    if (selected == BOOT_TARGET_NONE) {
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    boot_menu_show_loading(selected);
    load_result = boot_target_load(&context, selected);
    if (load_result != LOADER_OK) {
        boot_menu_show_error(selected, load_result);
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    /*
     * Phase 0.4D1 same-word patch control: preload and validate DPOSDBG.BIN,
     * rewrite the original EXE2 instruction with the same value, and run the
     * original patch cache-maintenance sequence without changing control flow. Failure or absence must
     * never block the stock oscilloscope boot path.
     */
    if (selected == BOOT_TARGET_ORIGINAL_SCOPE) {
        dposdbg_result = dposdbg_load_and_patch();

        console_set_color(WHITE);

        if (dposdbg_result == LOADER_OK)
            console_putln("DPOSDBG PRELOAD: OK");
        else
            console_putln("DPOSDBG PRELOAD: FAILED");

        if (*(volatile const uint32_t *)(uintptr_t)DPOSDBG_LOAD_ADDRESS ==
            DPOSDBG_MAGIC)
            console_putln("DPOSDBG MAGIC: DBG0");
        else
            console_putln("DPOSDBG MAGIC: BAD");

        if (dposdbg_result == LOADER_OK)
            console_putln("PATCH READBACK: OK");
        else
            console_putln("PATCH READBACK: FAILED");
        dposdbg_show_diag(dposdbg_result);
        console_present();

    }

    /*
     * Temporary DEBUG EXE2 checkpoint.
     * Reaching this screen proves that the complete FAT-loaded EXE2
     * matched the stock SPI-NOR payload byte for byte.
     */

    image = boot_target_image(&context, selected);
    if (!image || !loader_image_ready(image)) {
        boot_menu_show_error(selected, LOADER_ERR_NOT_LOADED);
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    /*
     * DEBUG EXE2 timing checkpoint.
     *
     * No artificial delay is used here. Once this screen is visible,
     * FAT loading has completed and the next operation is the normal
     * cache-safe application handoff.
     */
    if (selected == BOOT_TARGET_DEBUG_EXE2_FAT) {
        console_clear();
        console_set_color(WHITE);
        console_putln("EXE2 IN RAM");
        console_putln("LOADING DPOSLAB STAGE 3");
        console_present();
        load_result = dposlab_load(&context.debug_exe2, &dposlab_image);
        if (load_result != LOADER_OK) {
            boot_menu_show_error(selected, load_result);
            for (;;) __asm__ volatile ("wfi");
        }
        dposlab_handoff.magic = DPOSLAB_HANDOFF_MAGIC;
        dposlab_handoff.version = DPOSLAB_HANDOFF_VERSION;
        dposlab_handoff.size = (uint32_t)sizeof(dposlab_handoff);
        dposlab_handoff.flags = DPOSLAB_FLAG_EXE2_LOADED;
        dposlab_handoff.stage2_sp = 0U;
        dposlab_handoff.exe2_base = context.debug_exe2.image.load_address;
        dposlab_handoff.exe2_size = context.debug_exe2.image.image_size;
        dposlab_handoff.exe2_entry = 0x40000000U;
        dposlab_handoff.dposlab_base = dposlab_image.load_address;
        dposlab_handoff.dposlab_size = dposlab_image.image_size;
        dposlab_handoff.dposlab_entry = dposlab_image.entry_address;

        dposlab_start(&dposlab_image, &dposlab_handoff);
    }

    loader_start_image(image);
}
