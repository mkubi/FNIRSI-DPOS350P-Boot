#include <stddef.h>
#include <stdint.h>
#include "custom.h"

static const char custom_app_name[11] = CUSTOM_APP_NAME_83;

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void clear_app(custom_app_t *app)
{
    app->image.source = LOADER_SOURCE_NONE;
    app->image.load_address = 0U;
    app->image.entry_address = 0U;
    app->image.image_size = 0U;
    app->image.bytes_loaded = 0U;
    app->image.first_word = 0U;
    app->sectors_to_load = 0U;
    app->probed = 0U;
    app->loaded = 0U;
}

int custom_app_probe(custom_app_t *app)
{
    int r;

    if (!app)
        return LOADER_ERR_ARGUMENT;

    clear_app(app);

    r = sdmmc0_init(&app->sd_info);
    if (r)
        return LOADER_ERR_READ;

    r = fat_mount_first(&app->volume);
    if (r)
        return LOADER_ERR_READ;

    r = fat_find_root(&app->volume, custom_app_name, &app->file);
    if (r)
        return LOADER_ERR_NOT_FOUND;

    if (app->file.size < 4U || app->file.size > CUSTOM_APP_MAX_SIZE)
        return LOADER_ERR_BAD_SIZE;

    if (CUSTOM_APP_LOAD_ADDRESS + app->file.size < CUSTOM_APP_LOAD_ADDRESS)
        return LOADER_ERR_ADDRESS_RANGE;

    app->image.source = LOADER_SOURCE_FAT;
    app->image.load_address = CUSTOM_APP_LOAD_ADDRESS;
    app->image.entry_address = CUSTOM_APP_ENTRY_ADDRESS;
    app->image.image_size = app->file.size;
    app->image.bytes_loaded = 0U;
    app->image.first_word = 0U;
    app->sectors_to_load = (app->file.size + FAT_SECTOR_SIZE - 1U)
                         / FAT_SECTOR_SIZE;
    app->probed = 1U;
    return LOADER_OK;
}

int custom_app_load(custom_app_t *app)
{
    uint8_t *destination;
    uint32_t sector;
    uint32_t first_word;
    int r;

    if (!app || !app->probed)
        return LOADER_ERR_ARGUMENT;

    destination = (uint8_t *)(uintptr_t)app->image.load_address;

    /*
     * The target address is 32-bit aligned. fat_read_file_sector() and
     * sdmmc0_read_sector() copy one complete 512-byte sector at a time.
     * The final sector may contain FAT padding after image_size; execution
     * and cache maintenance still use the exact file size.
     */
    for (sector = 0U; sector < app->sectors_to_load; sector++) {
        r = fat_read_file_sector(&app->volume, &app->file, sector,
                                 destination + sector * FAT_SECTOR_SIZE);
        if (r) {
            app->image.bytes_loaded = sector * FAT_SECTOR_SIZE;
            return LOADER_ERR_READ;
        }
    }

    first_word = read_le32(destination);
    app->image.first_word = first_word;
    app->image.bytes_loaded = app->image.image_size;

    /* Known ARM-state startup signature of the current DPOSAPP.BIN test app. */
    if (first_word != 0xE59FD08CU)
        return LOADER_ERR_VERIFY;

    app->loaded = 1U;
    return LOADER_OK;
}
