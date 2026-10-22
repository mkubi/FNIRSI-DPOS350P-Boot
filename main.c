#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"
#include "gt911.h"
#include "boot_targets.h"
#include "boot_menu.h"
#include "start.h"

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

    image = boot_target_image(&context, selected);
    if (!image || !loader_image_ready(image)) {
        boot_menu_show_error(selected, LOADER_ERR_NOT_LOADED);
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    loader_start_image(image);
}
