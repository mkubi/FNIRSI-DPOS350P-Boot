#include <stdint.h>
#include "result.h"
#include "spi_nor_ro.h"

extern uint8_t __image_start;

static volatile stage1_load_result_t *const result =
    (volatile stage1_load_result_t *)(uintptr_t)STAGE1_RESULT_ADDR;

static void barrier(void)
{
    __asm__ volatile ("dsb sy\nisb sy" ::: "memory");
}

static uint32_t read_dram_size_from_xfel_slot(void)
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

static uint32_t read_le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void copy_to_result(volatile uint8_t *destination,
                           const uint8_t *source,
                           uint32_t length)
{
    uint32_t i;

    for (i = 0U; i < length; i++)
        destination[i] = source[i];
}

static uint32_t checksum_bytes(const uint8_t *data, uint32_t length)
{
    uint32_t sum = 0U;
    uint32_t i;

    for (i = 0U; i < length; i++)
        sum += data[i];

    return sum;
}

static int load_exe2_payload(uint32_t *loaded, uint32_t *checksum)
{
    uint8_t *destination = (uint8_t *)(uintptr_t)EXE2_LOAD_ADDRESS;
    uint8_t buffer[60];
    uint32_t source = EXE2_PAYLOAD_OFFSET;
    uint32_t remaining = EXE2_PAYLOAD_SIZE;
    uint32_t total = 0U;
    uint32_t sum = 0U;
    uint32_t chunk;
    uint32_t i;
    int status;

    while (remaining != 0U) {
        chunk = remaining > sizeof(buffer) ? (uint32_t)sizeof(buffer) : remaining;
        status = spi_nor_read(source, buffer, chunk);
        if (status != 0) {
            *loaded = total;
            *checksum = sum;
            return status;
        }

        for (i = 0U; i < chunk; i++)
            destination[total + i] = buffer[i];

        sum += checksum_bytes(buffer, chunk);
        source += chunk;
        total += chunk;
        remaining -= chunk;
    }

    barrier();
    *loaded = total;
    *checksum = sum;
    return 0;
}

void stage1_load_report(uint32_t original_sp, uint32_t original_lr)
{
    spi_nor_jedec_id_t jedec;
    uint8_t header[VERIFY_PREFIX_SIZE];
    const uint8_t *payload = (const uint8_t *)(uintptr_t)EXE2_LOAD_ADDRESS;
    uint32_t loaded = 0U;
    uint32_t checksum = 0U;

    result->magic = STAGE1_LOAD_MAGIC;
    result->state = STAGE1_LOAD_ENTER;
    result->image_base = (uint32_t)(uintptr_t)&__image_start;
    result->original_sp = original_sp;
    result->original_lr = original_lr;
    result->dram_size_bytes = read_dram_size_from_xfel_slot();
    result->source_offset = EXE2_PAYLOAD_OFFSET;
    result->destination_address = EXE2_LOAD_ADDRESS;
    result->expected_size = EXE2_PAYLOAD_SIZE;
    result->sentinel_a = 0xA55A5AA5U;
    result->sentinel_b = 0x5AA5A55AU;

    result->spi_init_result = spi_nor_ro_init();
    if (result->spi_init_result != 0) {
        result->jedec_result = -100;
        result->header_read_result = -100;
        result->payload_load_result = -100;
        result->state = STAGE1_LOAD_DONE;
        barrier();
        return;
    }

    result->jedec_result = spi_nor_read_jedec_id(&jedec);
    if (result->jedec_result == 0) {
        result->jedec_id = ((uint32_t)jedec.manufacturer << 16) |
                           ((uint32_t)jedec.memory_type << 8) |
                           (uint32_t)jedec.capacity;
    }

    result->header_read_result =
        spi_nor_read(EXE2_HEADER_OFFSET, header, sizeof(header));
    if (result->header_read_result == 0)
        copy_to_result(result->header_prefix, header, sizeof(header));

    result->payload_load_result = load_exe2_payload(&loaded, &checksum);
    result->bytes_loaded = loaded;
    result->payload_checksum = checksum;

    if (result->payload_load_result == 0 && loaded == EXE2_PAYLOAD_SIZE) {
        result->first_word = read_le32(payload);
        result->last_word = read_le32(payload + EXE2_PAYLOAD_SIZE - 4U);
        copy_to_result(result->payload_prefix,
                       payload,
                       VERIFY_PREFIX_SIZE);
        copy_to_result(result->payload_suffix,
                       payload + EXE2_PAYLOAD_SIZE - VERIFY_SUFFIX_SIZE,
                       VERIFY_SUFFIX_SIZE);
    }

    result->state = STAGE1_LOAD_DONE;
    barrier();
}
