#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"
#include "gt911.h"
#include "boot_targets.h"
#include "boot_menu.h"
#include "start.h"
#include "dposdbg.h"

#define BLACK 0x0000U
#define WHITE 0xFFFFU

int main(void)
{
    static boot_context_t context;
    gt911_info_t touch_info;
    boot_target_id_t selected;
    const loader_image_t *image;
    int touch_available;
    int load_result;
    int dposdbg_result;

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
	console_putln("HOOK SITE: SAME WORD");
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
        console_putln("DEBUG EXE2 IN RAM");
        console_putln("HANDOFF NOW");
        console_present();
    }

    loader_start_image(image);
}
