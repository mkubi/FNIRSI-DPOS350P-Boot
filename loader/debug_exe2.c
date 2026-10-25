#include <stddef.h>
#include <stdint.h>

#include "debug_exe2.h"
#include "cache.h"

#define DEBUG_EXE2_VERIFY_BYTES 32U

static void write_resident_test_pattern(void)
{
    volatile uint32_t *destination;
    uint32_t i;

    destination =
        (volatile uint32_t *)(uintptr_t)DEBUG_RESIDENT_TEST_ADDRESS;

    destination[0] = 0x32445352U; /* "RSD2" */
    destination[1] = DEBUG_RESIDENT_TEST_ADDRESS;
    destination[2] = DEBUG_RESIDENT_TEST_SIZE;
    destination[3] = 0x13579BDFU;

    for (i = 4U; i < DEBUG_RESIDENT_TEST_SIZE / 4U; i++)
        destination[i] = 0xA5000000U | i;
}

static const char debug_exe2_name[11] = DEBUG_EXE2_NAME_83;

static const uint8_t debug_exe2_prefix[DEBUG_EXE2_VERIFY_BYTES] = {
    0x18U, 0xF0U, 0x9FU, 0xE5U,
    0x18U, 0xF0U, 0x9FU, 0xE5U,
    0x18U, 0xF0U, 0x9FU, 0xE5U,
    0x18U, 0xF0U, 0x9FU, 0xE5U,
    0x18U, 0xF0U, 0x9FU, 0xE5U,
    0x00U, 0x00U, 0x00U, 0x00U,
    0x18U, 0xF0U, 0x9FU, 0xE5U,
    0x18U, 0xF0U, 0x9FU, 0xE5U
};

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int bytes_equal(const uint8_t *a, const uint8_t *b, uint32_t count)
{
    uint32_t i;

    for (i = 0U; i < count; i++) {
        if (a[i] != b[i])
            return 0;
    }

    return 1;
}

static void clear_app(debug_exe2_app_t *app)
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

int debug_exe2_probe(debug_exe2_app_t *app)
{
    static uint32_t sector_words[FAT_SECTOR_SIZE / 4U];
    const uint8_t *sector = (const uint8_t *)sector_words;
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

    r = fat_find_root(&app->volume, debug_exe2_name, &app->file);
    if (r)
        return LOADER_ERR_NOT_FOUND;

    if (app->file.size != DEBUG_EXE2_EXPECTED_SIZE)
        return LOADER_ERR_BAD_SIZE;

    if (DEBUG_EXE2_LOAD_ADDRESS + app->file.size <
        DEBUG_EXE2_LOAD_ADDRESS)
        return LOADER_ERR_ADDRESS_RANGE;

    r = fat_read_file_sector(&app->volume, &app->file, 0U,
                             (uint8_t *)sector_words);
    if (r)
        return LOADER_ERR_READ;

    if (!bytes_equal(sector, debug_exe2_prefix,
                     DEBUG_EXE2_VERIFY_BYTES))
        return LOADER_ERR_BAD_HEADER;

    app->image.source = LOADER_SOURCE_FAT;
    app->image.load_address = DEBUG_EXE2_LOAD_ADDRESS;
    app->image.entry_address = DEBUG_EXE2_ENTRY_ADDRESS;
    app->image.image_size = app->file.size;
    app->image.bytes_loaded = 0U;
    app->image.first_word = read_le32(sector);
    app->sectors_to_load = (app->file.size + FAT_SECTOR_SIZE - 1U)
                         / FAT_SECTOR_SIZE;
    app->probed = 1U;

    return LOADER_OK;
}

int debug_exe2_load(debug_exe2_app_t *app)
{
    static uint32_t last_sector_words[FAT_SECTOR_SIZE / 4U];
    uint8_t *last_sector = (uint8_t *)last_sector_words;
    fat_file_reader_t reader;
    uint8_t *destination;
    uint32_t offset = 0U;
    uint32_t remaining;
    uint32_t sector;
    uint32_t copy_bytes;
    uint32_t i;
    int r;

    if (!app || !app->probed)
        return LOADER_ERR_ARGUMENT;

    destination = (uint8_t *)(uintptr_t)app->image.load_address;

    r = fat_file_reader_init(&reader, &app->volume, &app->file);
    if (r)
        return LOADER_ERR_READ;

    for (sector = 0U; sector < app->sectors_to_load; ++sector) {
        remaining = app->image.image_size - offset;

        if (remaining >= FAT_SECTOR_SIZE) {
            r = fat_file_reader_read_next(&reader, destination + offset);
            copy_bytes = FAT_SECTOR_SIZE;
        } else {
            r = fat_file_reader_read_next(&reader, last_sector);
            copy_bytes = remaining;

            if (!r) {
                for (i = 0U; i < copy_bytes; ++i)
                    destination[offset + i] = last_sector[i];
            }
        }

        if (r) {
            app->image.bytes_loaded = offset;
            return LOADER_ERR_READ;
        }

        offset += copy_bytes;
    }

    if (offset != app->image.image_size)
        return LOADER_ERR_BAD_SIZE;

    if (!bytes_equal(destination, debug_exe2_prefix,
                     DEBUG_EXE2_VERIFY_BYTES))
        return LOADER_ERR_VERIFY;

    app->image.first_word = read_le32(destination);
    app->image.bytes_loaded = app->image.image_size;

    /*
     * Publish a known sentinel above the stock EXE2 ZI clear end
     * (0x42E68D24).  This is intentionally limited to the DEBUG EXE2
     * target; the Original App and DPOSDBG paths are left unchanged.
     */
    write_resident_test_pattern();
    cache_clean_range(DEBUG_RESIDENT_TEST_ADDRESS,
                      DEBUG_RESIDENT_TEST_SIZE);

    app->loaded = 1U;

    return LOADER_OK;
}
