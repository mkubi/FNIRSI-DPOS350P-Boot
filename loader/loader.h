#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>

typedef enum {
    LOADER_SOURCE_NONE = 0,
    LOADER_SOURCE_FAT,
    LOADER_SOURCE_SPI_NOR
} loader_source_t;

typedef enum {
    LOADER_OK = 0,
    LOADER_ERR_ARGUMENT = -1,
    LOADER_ERR_NOT_FOUND = -2,
    LOADER_ERR_BAD_SIZE = -3,
    LOADER_ERR_READ = -4,
    LOADER_ERR_ADDRESS_RANGE = -5,
    LOADER_ERR_VERIFY = -6,
    LOADER_ERR_NOT_LOADED = -7
} loader_result_t;

typedef struct {
    loader_source_t source;
    uint32_t load_address;
    uint32_t entry_address;
    uint32_t image_size;
    uint32_t bytes_loaded;
    uint32_t first_word;
} loader_image_t;

#endif /* LOADER_H */
