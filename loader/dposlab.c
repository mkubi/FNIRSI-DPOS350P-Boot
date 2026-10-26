#include <stdint.h>

#include "dposlab.h"
#include "fat.h"
#include "cache.h"

static const char name83[11] = DPOSLAB_NAME_83;

int dposlab_load(debug_exe2_app_t *exe2, loader_image_t *image)
{
    static uint32_t last_sector_words[FAT_SECTOR_SIZE / 4U];
    uint8_t *last_sector = (uint8_t *)last_sector_words;
    fat_file_t file;
    fat_file_reader_t reader;
    uint8_t *destination =
        (uint8_t *)(uintptr_t)DPOSLAB_LOAD_ADDRESS;
    uint32_t offset = 0U;
    uint32_t remaining;
    int r;

    if (!exe2 || !image || !exe2->loaded)
        return LOADER_ERR_ARGUMENT;

    image->source = LOADER_SOURCE_NONE;
    image->load_address = 0U;
    image->entry_address = 0U;
    image->image_size = 0U;
    image->bytes_loaded = 0U;
    image->first_word = 0U;

    r = fat_find_root(&exe2->volume, name83, &file);
    if (r)
        return LOADER_ERR_NOT_FOUND;

    if (!file.size || file.size > DPOSLAB_MAX_SIZE)
        return LOADER_ERR_BAD_SIZE;

    r = fat_file_reader_init(&reader, &exe2->volume, &file);
    if (r)
        return LOADER_ERR_READ;

    remaining = file.size;

    while (remaining != 0U) {
        uint32_t chunk = remaining;

        if (chunk > FAT_SECTOR_SIZE)
            chunk = FAT_SECTOR_SIZE;

        if (chunk == FAT_SECTOR_SIZE) {
            r = fat_file_reader_read_next(
                &reader,
                destination + offset);
        } else {
            uint32_t i;

            r = fat_file_reader_read_next(
                &reader,
                last_sector);

            if (!r) {
                for (i = 0U; i < chunk; ++i)
                    destination[offset + i] = last_sector[i];
            }
        }

        if (r)
            return LOADER_ERR_READ;

        offset += chunk;
        remaining -= chunk;
    }

    image->source = LOADER_SOURCE_FAT;
    image->load_address = DPOSLAB_LOAD_ADDRESS;
    image->entry_address = DPOSLAB_LOAD_ADDRESS;
    image->image_size = file.size;
    image->bytes_loaded = file.size;
    image->first_word =
        *(const uint32_t *)(const void *)destination;

    cache_clean_range(DPOSLAB_LOAD_ADDRESS, file.size);

    return LOADER_OK;
}


extern void dposlab_jump_stage3(uint32_t entry,
                                dposlab_handoff_t *handoff)
    __attribute__((noreturn));

void dposlab_start(const loader_image_t *image, dposlab_handoff_t *handoff)
{
    uint32_t stage2_sp;

    if (!image || !handoff ||
        image->source == LOADER_SOURCE_NONE ||
        image->bytes_loaded != image->image_size) {
        for (;;)
            __asm__ volatile ("wfi");
    }

    cache_clean_range((uintptr_t)image->load_address, image->image_size);
    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    /* Capture the actual Stage 2 stack in the final C handoff frame. */
    __asm__ volatile ("mov %0, sp" : "=r" (stage2_sp));
    handoff->stage2_sp = stage2_sp;
    cache_clean_range((uintptr_t)handoff, sizeof(*handoff));
    cache_dsb();

    dposlab_jump_stage3(image->entry_address, handoff);
}
