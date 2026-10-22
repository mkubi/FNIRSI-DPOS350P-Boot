#ifndef STAGE1_RESULT_H
#define STAGE1_RESULT_H

#include <stdint.h>

#define STAGE1_RESULT_ADDR 0x00009F80U
#define STAGE1_SPI_MAGIC   0x31495053U /* "SPI1" little-endian */
#define STAGE1_SPI_ENTER   0x00000001U
#define STAGE1_SPI_DONE    0x00000002U

#define SPI_PREFIX_SIZE    16U

typedef struct {
    uint32_t magic;
    uint32_t state;
    uint32_t image_base;
    uint32_t original_sp;
    uint32_t original_lr;
    uint32_t dram_size_bytes;

    int32_t spi_init_result;
    int32_t jedec_result;
    uint32_t jedec_id;

    int32_t bt0_read_result;
    int32_t exe1_read_result;
    int32_t exe2_read_result;
    int32_t upper_read_result;

    uint8_t bt0_prefix[SPI_PREFIX_SIZE];
    uint8_t exe1_prefix[SPI_PREFIX_SIZE];
    uint8_t exe2_prefix[SPI_PREFIX_SIZE];
    uint8_t upper_prefix[SPI_PREFIX_SIZE];

    uint32_t sentinel_a;
    uint32_t sentinel_b;
} stage1_spi_result_t;

_Static_assert(sizeof(stage1_spi_result_t) == 0x7CU,
               "Unexpected Stage 1B.1 result size");

#endif /* STAGE1_RESULT_H */
