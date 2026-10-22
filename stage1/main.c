#include <stdint.h>
#include "result.h"
#include "spi_nor_ro.h"

extern uint8_t __image_start;

extern void stage2_handoff(uint32_t entry_address)
    __attribute__((noreturn));

static volatile stage1_stg2_result_t *const result =
    (volatile stage1_stg2_result_t *)(uintptr_t)STAGE1_RESULT_ADDR;

static void barrier(void)
{
    __asm__ volatile ("dsb sy\nisb sy" ::: "memory");
}

static uint32_t read_dram_size(void)
{
    uint32_t value;
    uint32_t address = 0x00000020U;

    __asm__ volatile (
        "ldr %0, [%1]"
        : "=r" (value)
        : "r" (address)
        : "memory"
    );

    return value;
}

static void copy_bytes(volatile uint8_t *destination,
                       const uint8_t *source, uint32_t length)
{
    uint32_t i;
    for (i = 0U; i < length; ++i)
        destination[i] = source[i];
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data,
                             uint32_t length)
{
    uint32_t i;
    unsigned int bit;

    for (i = 0U; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U &
                  (0U - (crc & 1U)));
    }
    return crc;
}

static int validate_header(const stage2_header_t *header)
{
    uint32_t payload_end;

    if (header->magic != STAGE2_MAGIC)
        return -10;
    if (header->version != STAGE2_HEADER_VERSION)
        return -11;
    if (header->header_size != STAGE2_HEADER_SIZE)
        return -12;
    if (header->payload_size == 0U ||
        header->payload_size > STAGE2_MAX_PAYLOAD_SIZE)
        return -13;
    if (header->load_address != STAGE2_LOAD_ADDRESS)
        return -14;

    payload_end = header->load_address + header->payload_size;
    if (payload_end < header->load_address || payload_end > 0x44000000U)
        return -15;
    if (header->entry_address < header->load_address ||
        header->entry_address >= payload_end)
        return -16;
    if (header->flags != 0U)
        return -17;

    return 0;
}

static int load_payload(const stage2_header_t *header,
                        uint32_t *bytes_loaded, uint32_t *calculated_crc)
{
    uint8_t buffer[60];
    uint8_t *destination =
        (uint8_t *)(uintptr_t)header->load_address;
    uint32_t source = STAGE2_NOR_OFFSET + header->header_size;
    uint32_t remaining = header->payload_size;
    uint32_t total = 0U;
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t chunk;
    uint32_t i;
    int status;

    while (remaining != 0U) {
        chunk = remaining > sizeof(buffer) ?
                (uint32_t)sizeof(buffer) : remaining;
        status = spi_nor_read(source, buffer, chunk);
        if (status != 0) {
            *bytes_loaded = total;
            *calculated_crc = crc ^ 0xFFFFFFFFU;
            return status;
        }

        for (i = 0U; i < chunk; ++i)
            destination[total + i] = buffer[i];

        crc = crc32_update(crc, buffer, chunk);
        source += chunk;
        total += chunk;
        remaining -= chunk;
    }

    barrier();
    *bytes_loaded = total;
    *calculated_crc = crc ^ 0xFFFFFFFFU;
    return (*calculated_crc == header->payload_crc32) ? 0 : -20;
}

void stage1_stg2_report(uint32_t original_sp, uint32_t original_lr)
{
    stage2_header_t header;
    spi_nor_jedec_id_t jedec;
    const uint8_t *payload;
    uint32_t loaded = 0U;
    uint32_t crc = 0U;

    result->magic = STAGE1_RESULT_MAGIC;
    result->state = STAGE1_STATE_ENTER;
    result->image_base = (uint32_t)(uintptr_t)&__image_start;
    result->original_sp = original_sp;
    result->original_lr = original_lr;
    result->dram_size_bytes = read_dram_size();
    result->nor_offset = STAGE2_NOR_OFFSET;
    result->slot_size = STAGE2_SLOT_SIZE;
    result->sentinel_a = 0xA55A5AA5U;
    result->sentinel_b = 0x5AA5A55AU;

    result->spi_init_result = spi_nor_ro_init();
    if (result->spi_init_result != 0)
        goto done;

    result->jedec_result = spi_nor_read_jedec_id(&jedec);
    if (result->jedec_result == 0) {
        result->jedec_id = ((uint32_t)jedec.manufacturer << 16) |
                           ((uint32_t)jedec.memory_type << 8) |
                           (uint32_t)jedec.capacity;
    } else {
        goto done;
    }

    result->header_read_result =
        spi_nor_read(STAGE2_NOR_OFFSET, (uint8_t *)&header,
                     sizeof(header));
    if (result->header_read_result != 0)
        goto done;

    result->header_magic = header.magic;
    result->header_version = header.version;
    result->header_size = header.header_size;
    result->payload_size = header.payload_size;
    result->load_address = header.load_address;
    result->entry_address = header.entry_address;
    result->expected_crc32 = header.payload_crc32;
    result->flags = header.flags;

    result->header_validate_result = validate_header(&header);
    if (result->header_validate_result != 0)
        goto done;

    result->payload_load_result = load_payload(&header, &loaded, &crc);
    result->bytes_loaded = loaded;
    result->calculated_crc32 = crc;

    if (result->payload_load_result == 0) {
        payload = (const uint8_t *)(uintptr_t)header.load_address;
        copy_bytes(result->payload_prefix, payload, 16U);
        copy_bytes(result->payload_suffix,
                   payload + header.payload_size - 16U, 16U);
    }

done:
    result->state = STAGE1_STATE_DONE;
    barrier();

    if (result->header_validate_result == 0 &&
        result->payload_load_result == 0 &&
        result->bytes_loaded == header.payload_size &&
        result->calculated_crc32 == header.payload_crc32) {
        stage2_handoff(header.entry_address);
    }
}
