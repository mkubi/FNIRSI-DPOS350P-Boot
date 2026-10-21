#ifndef LOADER_ORIGINAL_H
#define LOADER_ORIGINAL_H
#include <stdint.h>

typedef enum {
    ORIGINAL_LOAD_OK = 0,
    ORIGINAL_LOAD_ERR_HEADER_READ = -1,
    ORIGINAL_LOAD_ERR_HEADER = -2,
    ORIGINAL_LOAD_ERR_LENGTH = -3,
    ORIGINAL_LOAD_ERR_IMAGE_READ = -4
} original_load_result_t;

int original_load(uint32_t *image_length);
void original_jump(void) __attribute__((noreturn));

#endif
