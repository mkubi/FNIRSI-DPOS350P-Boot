#ifndef STAGE1_RESULT_H
#define STAGE1_RESULT_H

#include <stdint.h>

#define STAGE1_RESULT_ADDR      0x00009F70U
#define STAGE1_RESULT_MAGIC     0x34475453U /* "STG4" */
#define STAGE1_STATE_ENTER      0x00000001U
#define STAGE1_STATE_DONE       0x00000002U

#define STAGE2_NOR_OFFSET       0x001DB000U
#define STAGE2_SLOT_SIZE        0x00010000U
#define STAGE2_MAGIC            0x32475453U /* "STG2" little-endian */
#define STAGE2_HEADER_VERSION   1U
#define STAGE2_HEADER_SIZE      32U
#define STAGE2_LOAD_ADDRESS     0x41000000U
#define STAGE2_MAX_PAYLOAD_SIZE (STAGE2_SLOT_SIZE - STAGE2_HEADER_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t payload_size;
    uint32_t load_address;
    uint32_t entry_address;
    uint32_t payload_crc32;
    uint32_t flags;
} stage2_header_t;

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
    int32_t header_validate_result;
    int32_t payload_load_result;
    uint32_t bytes_loaded;

    uint32_t nor_offset;
    uint32_t slot_size;
    uint32_t header_magic;
    uint32_t header_version;
    uint32_t header_size;
    uint32_t payload_size;
    uint32_t load_address;
    uint32_t entry_address;
    uint32_t expected_crc32;
    uint32_t calculated_crc32;
    uint32_t flags;

    uint8_t payload_prefix[16];
    uint8_t payload_suffix[16];

    uint32_t sentinel_a;
    uint32_t sentinel_b;
} stage1_stg2_result_t;

_Static_assert(sizeof(stage2_header_t) == STAGE2_HEADER_SIZE,
               "Unexpected Stage 2 header size");
_Static_assert(sizeof(stage1_stg2_result_t) == 0x88U,
               "Unexpected Stage 1B.3 result size");

#endif
