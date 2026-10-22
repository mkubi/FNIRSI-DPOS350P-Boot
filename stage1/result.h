#ifndef STAGE1_RESULT_H
#define STAGE1_RESULT_H

#include <stdint.h>

#define STAGE1_RESULT_ADDR    0x00009F80U
#define STAGE1_LOAD_MAGIC     0x32444F4CU /* "LOD2" little-endian */
#define STAGE1_LOAD_ENTER     0x00000001U
#define STAGE1_LOAD_DONE      0x00000002U

#define EXE2_HEADER_OFFSET    0x000A8000U
#define EXE2_PAYLOAD_OFFSET   0x000A8020U
#define EXE2_PAYLOAD_SIZE     0x000957E0U
#define EXE2_LOAD_ADDRESS     0x40000000U

#define VERIFY_PREFIX_SIZE    16U
#define VERIFY_SUFFIX_SIZE    16U

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

    int32_t header_read_result;
    int32_t payload_load_result;
    uint32_t bytes_loaded;
    uint32_t source_offset;
    uint32_t destination_address;
    uint32_t expected_size;

    uint32_t payload_checksum;
    uint32_t first_word;
    uint32_t last_word;

    uint8_t header_prefix[VERIFY_PREFIX_SIZE];
    uint8_t payload_prefix[VERIFY_PREFIX_SIZE];
    uint8_t payload_suffix[VERIFY_SUFFIX_SIZE];

    uint32_t sentinel_a;
    uint32_t sentinel_b;
} stage1_load_result_t;

_Static_assert(sizeof(stage1_load_result_t) == 0x80U,
               "Unexpected Stage 1B.2 result size");

#endif /* STAGE1_RESULT_H */
