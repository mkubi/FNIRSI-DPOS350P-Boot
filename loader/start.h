#ifndef LOADER_START_H
#define LOADER_START_H

#include "loader.h"

int loader_image_ready(const loader_image_t *image);
void loader_start_image(const loader_image_t *image)
    __attribute__((noreturn));

#endif /* LOADER_START_H */
