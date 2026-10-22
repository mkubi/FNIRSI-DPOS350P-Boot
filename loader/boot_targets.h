#ifndef BOOT_TARGETS_H
#define BOOT_TARGETS_H

#include <stdint.h>
#include "custom.h"
#include "original.h"

typedef enum {
    BOOT_TARGET_CUSTOM = 0,
    BOOT_TARGET_ORIGINAL_SCOPE,
    BOOT_TARGET_USB_CONNECT,
    BOOT_TARGET_COUNT,
    BOOT_TARGET_NONE = 0xFF
} boot_target_id_t;

typedef struct {
    uint8_t available;
    int probe_result;
} boot_target_status_t;

typedef struct {
    boot_target_status_t status[BOOT_TARGET_COUNT];
    custom_app_t custom;
    original_app_t scope;
    original_app_t usb;
} boot_context_t;

const char *boot_target_name(boot_target_id_t id);
void boot_targets_probe(boot_context_t *context);
boot_target_id_t boot_targets_default(const boot_context_t *context);
int boot_target_load(boot_context_t *context, boot_target_id_t id);
const loader_image_t *boot_target_image(const boot_context_t *context,
                                        boot_target_id_t id);

#endif /* BOOT_TARGETS_H */
