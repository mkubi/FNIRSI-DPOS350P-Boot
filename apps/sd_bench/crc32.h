#ifndef SD_BENCH_CRC32_H
#define SD_BENCH_CRC32_H

#include <stdint.h>

uint32_t sd_bench_crc32_update(uint32_t crc,
                               const void *data,
                               uint32_t size);

#endif
