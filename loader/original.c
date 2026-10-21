#include "original.h"
#include "image.h"
/* Legacy implementation, not currently built. */
/* #include "spinor.h" */
#include "cache.h"
#include "board/dpos350p/memory_map.h"

#define ORIGINAL_HEADER_READ_SIZE 64U
#define ORIGINAL_APP_MAX_SIZE     (16U * 1024U * 1024U)

int original_load(uint32_t *image_length)
{
    uint8_t header_buffer[ORIGINAL_HEADER_READ_SIZE];
    const egon_exec_header_t *header = (const egon_exec_header_t *)header_buffer;
    uint32_t length;

    if (spinor_read(ORIGINAL_APP_NOR_OFFSET,
                    header_buffer, sizeof(header_buffer)) != 0)
        return ORIGINAL_LOAD_ERR_HEADER_READ;

    if (!image_is_egon_exec(header_buffer, sizeof(header_buffer)))
        return ORIGINAL_LOAD_ERR_HEADER;

    length = header->length;
    if (length < EGON_HEADER_MIN_SIZE || length > ORIGINAL_APP_MAX_SIZE)
        return ORIGINAL_LOAD_ERR_LENGTH;

    if (spinor_read(ORIGINAL_APP_NOR_OFFSET,
                    (void *)(uintptr_t)ORIGINAL_APP_LOAD_ADDR,
                    length) != 0)
        return ORIGINAL_LOAD_ERR_IMAGE_READ;

    cache_clean_range(ORIGINAL_APP_LOAD_ADDR, length);
    cache_dsb();
    cache_isb();

    if (image_length)
        *image_length = length;

    return ORIGINAL_LOAD_OK;
}

void original_jump(void)
{
    void (*entry)(void) = (void (*)(void))(uintptr_t)ORIGINAL_APP_LOAD_ADDR;
    cache_dsb();
    cache_isb();
    entry();
    for (;;) { }
}
