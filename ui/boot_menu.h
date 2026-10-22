#ifndef BOOT_MENU_H
#define BOOT_MENU_H

#include <stdint.h>
#include "boot_targets.h"

#define BOOT_MENU_AUTOBOOT_SECONDS 5U

void boot_menu_show_loading(boot_target_id_t selected);
void boot_menu_show_error(boot_target_id_t selected, int error);
boot_target_id_t boot_menu_run(const boot_context_t *context,
                               int touch_available);

#endif /* BOOT_MENU_H */

