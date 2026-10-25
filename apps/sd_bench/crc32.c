#include <stdint.h>
#include "crc32.h"

uint32_t sd_bench_crc32_update(uint32_t crc,
                               const void *data,
                               uint32_t size)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t i;
    unsigned bit;

    for (i = 0U; i < size; ++i) {
        crc ^= p[i];
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }

    return crc;
}
