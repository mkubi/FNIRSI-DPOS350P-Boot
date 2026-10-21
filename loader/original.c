#include <stddef.h>
#include <stdint.h>
#include "original.h"
#include "spi_nor_ro.h"

#define EGON_HEADER_SIZE            0x20U
#define ORIGINAL_LOAD_ADDRESS       0x40000000U
#define ORIGINAL_ENTRY_ADDRESS      0x40000000U
#define ORIGINAL_MAX_TOTAL_SIZE     0x00200000U
#define ORIGINAL_VERIFY_BYTES       32U

#define EXE1_HEADER_OFFSET          0x00018000U
#define EXE1_EXPECTED_TOTAL_SIZE    0x00024800U

#define EXE2_HEADER_OFFSET          0x000A8000U
#define EXE2_EXPECTED_TOTAL_SIZE    0x00095800U

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

static int header_magic_valid(const uint8_t header[EGON_HEADER_SIZE])
{
    static const uint8_t magic[8] = {
        'e', 'G', 'O', 'N', '.', 'E', 'X', 'E'
    };
    static const uint8_t type[4] = {
        'E', 'X', 'E', 'C'
    };

    if (!bytes_equal(&header[4], magic, 8U))
        return 0;
    if (!bytes_equal(&header[20], type, 4U))
        return 0;
    return 1;
}

static void clear_app(original_app_t *app)
{
    app->image.source = LOADER_SOURCE_NONE;
    app->image.load_address = 0U;
    app->image.entry_address = 0U;
    app->image.image_size = 0U;
    app->image.bytes_loaded = 0U;
    app->image.first_word = 0U;
    app->header_offset = 0U;
    app->payload_offset = 0U;
    app->total_size = 0U;
    app->expected_total_size = 0U;
    app->header_branch = 0U;
    app->header_field_0c = 0U;
    app->probed = 0U;
    app->loaded = 0U;
}

const char *original_image_name(original_image_id_t id)
{
    if (id == ORIGINAL_IMAGE_EXE1_USB)
        return "EXE1 USB CONNECT";
    if (id == ORIGINAL_IMAGE_EXE2_SCOPE)
        return "EXE2 OSCILLOSCOPE";
    return "UNKNOWN";
}

int original_app_probe(original_image_id_t id, original_app_t *app)
{
    uint8_t header[EGON_HEADER_SIZE];
    uint32_t expected_size;
    uint32_t offset;
    uint32_t total_size;
    int r;

    if (!app)
        return LOADER_ERR_ARGUMENT;

    clear_app(app);
    app->id = id;

    if (id == ORIGINAL_IMAGE_EXE1_USB) {
        offset = EXE1_HEADER_OFFSET;
        expected_size = EXE1_EXPECTED_TOTAL_SIZE;
    } else if (id == ORIGINAL_IMAGE_EXE2_SCOPE) {
        offset = EXE2_HEADER_OFFSET;
        expected_size = EXE2_EXPECTED_TOTAL_SIZE;
    } else {
        return LOADER_ERR_ARGUMENT;
    }

    r = spi_nor_ro_init();
    if (r)
        return LOADER_ERR_READ;

    r = spi_nor_read_chunked(offset, header, EGON_HEADER_SIZE);
    if (r)
        return LOADER_ERR_READ;

    if (!header_magic_valid(header))
        return LOADER_ERR_BAD_HEADER;

    total_size = read_le32(&header[16]);
    if (total_size != expected_size)
        return LOADER_ERR_BAD_SIZE;
    if (total_size <= EGON_HEADER_SIZE ||
        total_size > ORIGINAL_MAX_TOTAL_SIZE)
        return LOADER_ERR_BAD_SIZE;

    app->header_offset = offset;
    app->payload_offset = offset + EGON_HEADER_SIZE;
    app->total_size = total_size;
    app->expected_total_size = expected_size;
    app->header_branch = read_le32(&header[0]);
    app->header_field_0c = read_le32(&header[12]);

    app->image.source = LOADER_SOURCE_SPI_NOR;
    app->image.load_address = ORIGINAL_LOAD_ADDRESS;
    app->image.entry_address = ORIGINAL_ENTRY_ADDRESS;
    app->image.image_size = total_size - EGON_HEADER_SIZE;
    app->image.bytes_loaded = 0U;
    app->image.first_word = 0U;
    app->probed = 1U;
    return LOADER_OK;
}

int original_app_load(original_app_t *app)
{
    uint8_t verify[ORIGINAL_VERIFY_BYTES];
    uint8_t *destination;
    uint32_t i;
    int r;

    if (!app || !app->probed)
        return LOADER_ERR_ARGUMENT;

    destination = (uint8_t *)(uintptr_t)app->image.load_address;

    r = spi_nor_read_chunked(app->payload_offset,
                             destination,
                             app->image.image_size);
    if (r)
        return LOADER_ERR_READ;

    /* Re-read the payload prefix directly from NOR and compare with RAM. */
    r = spi_nor_read_chunked(app->payload_offset,
                             verify,
                             ORIGINAL_VERIFY_BYTES);
    if (r)
        return LOADER_ERR_READ;

    for (i = 0U; i < ORIGINAL_VERIFY_BYTES; i++) {
        if (destination[i] != verify[i])
            return LOADER_ERR_VERIFY;
    }

    app->image.first_word = read_le32(destination);
    app->image.bytes_loaded = app->image.image_size;
    app->loaded = 1U;
    return LOADER_OK;
}
