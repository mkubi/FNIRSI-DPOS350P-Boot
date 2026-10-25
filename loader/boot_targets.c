#include <stddef.h>

#include "boot_targets.h"

static void clear_status(boot_target_status_t *status)
{
    status->available = 0U;
    status->probe_result = LOADER_ERR_NOT_FOUND;
}

const char *boot_target_name(boot_target_id_t id)
{
    switch (id) {
    case BOOT_TARGET_DEBUG_EXE2_FAT:
        return "DEBUG EXE2 FROM FAT";
    case BOOT_TARGET_CUSTOM:
        return "CUSTOM APPLICATION";
    case BOOT_TARGET_ORIGINAL_SCOPE:
        return "ORIGINAL OSCILLOSCOPE";
    case BOOT_TARGET_USB_CONNECT:
        return "USB CONNECT";
    default:
        return "UNKNOWN";
    }
}

void boot_targets_probe(boot_context_t *context)
{
    int result;
    unsigned i;

    if (!context)
        return;

    for (i = 0U; i < BOOT_TARGET_COUNT; i++)
        clear_status(&context->status[i]);

    result = debug_exe2_probe(&context->debug_exe2);
    context->status[BOOT_TARGET_DEBUG_EXE2_FAT].probe_result = result;
    context->status[BOOT_TARGET_DEBUG_EXE2_FAT].available =
        result == LOADER_OK;

    result = custom_app_probe(&context->custom);
    context->status[BOOT_TARGET_CUSTOM].probe_result = result;
    context->status[BOOT_TARGET_CUSTOM].available = result == LOADER_OK;

    result = original_app_probe(ORIGINAL_IMAGE_EXE2_SCOPE,
                                &context->scope);
    context->status[BOOT_TARGET_ORIGINAL_SCOPE].probe_result = result;
    context->status[BOOT_TARGET_ORIGINAL_SCOPE].available =
        result == LOADER_OK;

    result = original_app_probe(ORIGINAL_IMAGE_EXE1_USB,
                                &context->usb);
    context->status[BOOT_TARGET_USB_CONNECT].probe_result = result;
    context->status[BOOT_TARGET_USB_CONNECT].available =
        result == LOADER_OK;
}

boot_target_id_t boot_targets_default(const boot_context_t *context)
{
    if (!context)
        return BOOT_TARGET_NONE;

    if (context->status[BOOT_TARGET_DEBUG_EXE2_FAT].available)
        return BOOT_TARGET_DEBUG_EXE2_FAT;

    if (context->status[BOOT_TARGET_CUSTOM].available)
        return BOOT_TARGET_CUSTOM;

    if (context->status[BOOT_TARGET_ORIGINAL_SCOPE].available)
        return BOOT_TARGET_ORIGINAL_SCOPE;

    if (context->status[BOOT_TARGET_USB_CONNECT].available)
        return BOOT_TARGET_USB_CONNECT;

    return BOOT_TARGET_NONE;
}

int boot_target_load(boot_context_t *context, boot_target_id_t id)
{
    if (!context || id >= BOOT_TARGET_COUNT)
        return LOADER_ERR_ARGUMENT;

    if (!context->status[id].available)
        return context->status[id].probe_result;

    switch (id) {
    case BOOT_TARGET_DEBUG_EXE2_FAT:
        return debug_exe2_load(&context->debug_exe2);
    case BOOT_TARGET_CUSTOM:
        return custom_app_load(&context->custom);
    case BOOT_TARGET_ORIGINAL_SCOPE:
        return original_app_load(&context->scope);
    case BOOT_TARGET_USB_CONNECT:
        return original_app_load(&context->usb);
    default:
        return LOADER_ERR_ARGUMENT;
    }
}

const loader_image_t *boot_target_image(const boot_context_t *context,
                                        boot_target_id_t id)
{
    if (!context || id >= BOOT_TARGET_COUNT)
        return NULL;

    switch (id) {
    case BOOT_TARGET_DEBUG_EXE2_FAT:
        return &context->debug_exe2.image;
    case BOOT_TARGET_CUSTOM:
        return &context->custom.image;
    case BOOT_TARGET_ORIGINAL_SCOPE:
        return &context->scope.image;
    case BOOT_TARGET_USB_CONNECT:
        return &context->usb.image;
    default:
        return NULL;
    }
}
