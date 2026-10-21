#ifndef LOADER_IMAGE_H
#define LOADER_IMAGE_H

#include <stdint.h>

#define EGON_HEADER_MIN_SIZE 32U

typedef struct {
    uint32_t branch;
    char magic[8];
    uint32_t checksum;
    uint32_t length;
    char type[4];
    uint32_t reserved0;
    uint32_t reserved1;
} egon_exec_header_t;

int image_is_egon_exec(const void *buffer, uint32_t length);

#endif
