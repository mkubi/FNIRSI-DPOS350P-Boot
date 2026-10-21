#ifndef ORIGINAL_LOADER_H
#define ORIGINAL_LOADER_H

#include <stdint.h>
#include "loader.h"

typedef enum {
    ORIGINAL_IMAGE_EXE1_USB = 0,
    ORIGINAL_IMAGE_EXE2_SCOPE = 1
} original_image_id_t;

typedef struct {
    loader_image_t image;
    original_image_id_t id;
    uint32_t header_offset;
    uint32_t payload_offset;
    uint32_t total_size;
    uint32_t expected_total_size;
    uint32_t header_branch;
    uint32_t header_field_0c;
    uint8_t probed;
    uint8_t loaded;
} original_app_t;

const char *original_image_name(original_image_id_t id);
int original_app_probe(original_image_id_t id, original_app_t *app);
int original_app_load(original_app_t *app);

#endif /* ORIGINAL_LOADER_H */
