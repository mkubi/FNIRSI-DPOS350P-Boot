#include <stdint.h>
#include "cache.h"
#include "jump.h"
#include "start.h"

int loader_image_ready(const loader_image_t *image)
{
    if (!image)
        return 0;
    if (image->source == LOADER_SOURCE_NONE)
        return 0;
    if (!image->load_address || !image->entry_address)
        return 0;
    if (!image->image_size)
        return 0;
    if (image->bytes_loaded != image->image_size)
        return 0;
    if (image->entry_address < image->load_address)
        return 0;
    if (image->entry_address >= image->load_address + image->image_size)
        return 0;

    return 1;
}

void loader_start_image(const loader_image_t *image)
{
    if (!loader_image_ready(image)) {
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    /* Publish the freshly loaded code from D-cache to physical memory. */
    cache_clean_range((uintptr_t)image->load_address, image->image_size);

    /* Remove stale instructions and predictions before changing images. */
    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();

    cache_dsb();
    cache_isb();

    loader_jump(image->entry_address);
}
