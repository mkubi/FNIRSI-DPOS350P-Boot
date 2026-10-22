#include <stdint.h>
#include "result.h"
#include "spi_nor_ro.h"

extern uint8_t __image_start;

static volatile stage1_spi_result_t *const result =
    (volatile stage1_spi_result_t *)(uintptr_t)STAGE1_RESULT_ADDR;

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

static void copy_prefix(volatile uint8_t *destination,
                        const uint8_t *source)
{
    unsigned int i;

    for (i = 0U; i < SPI_PREFIX_SIZE; i++)
        destination[i] = source[i];
}

static int read_prefix(uint32_t address, volatile uint8_t *destination)
{
    uint8_t buffer[SPI_PREFIX_SIZE];
    int status;

    status = spi_nor_read(address, buffer, SPI_PREFIX_SIZE);
    if (status == 0)
        copy_prefix(destination, buffer);

    return status;
}

void stage1_spi_report(uint32_t original_sp, uint32_t original_lr)
{
    spi_nor_jedec_id_t jedec;

    result->magic = STAGE1_SPI_MAGIC;
    result->state = STAGE1_SPI_ENTER;
    result->image_base = (uint32_t)(uintptr_t)&__image_start;
    result->original_sp = original_sp;
    result->original_lr = original_lr;
    result->dram_size_bytes = read_dram_size_from_xfel_slot();
    result->sentinel_a = 0xA55A5AA5U;
    result->sentinel_b = 0x5AA5A55AU;

    result->spi_init_result = spi_nor_ro_init();

    if (result->spi_init_result == 0) {
        result->jedec_result = spi_nor_read_jedec_id(&jedec);
        if (result->jedec_result == 0) {
            result->jedec_id = ((uint32_t)jedec.manufacturer << 16) |
                               ((uint32_t)jedec.memory_type << 8) |
                               (uint32_t)jedec.capacity;
        }

        result->bt0_read_result =
            read_prefix(0x000000U, result->bt0_prefix);
        result->exe1_read_result =
            read_prefix(0x018000U, result->exe1_prefix);
        result->exe2_read_result =
            read_prefix(0x0A8000U, result->exe2_prefix);
        result->upper_read_result =
            read_prefix(0x140000U, result->upper_prefix);
    } else {
        result->jedec_result = -100;
        result->bt0_read_result = -100;
        result->exe1_read_result = -100;
        result->exe2_read_result = -100;
        result->upper_read_result = -100;
    }

    result->state = STAGE1_SPI_DONE;
    barrier();
}
