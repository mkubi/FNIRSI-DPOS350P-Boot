#ifndef CUSTOM_LOADER_H
#define CUSTOM_LOADER_H

#include <stdint.h>
#include "loader.h"
#include "fat.h"
#include "sdmmc0.h"

#define CUSTOM_APP_LOAD_ADDRESS  0x40000000U
#define CUSTOM_APP_ENTRY_ADDRESS 0x40000000U
#define CUSTOM_APP_MAX_SIZE      0x01000000U

/* FAT 8.3 representation of DPOSAPP.BIN. */
#define CUSTOM_APP_NAME_83 \
    { 'D','P','O','S','A','P','P',' ','B','I','N' }

typedef struct {
    loader_image_t image;
    sdmmc0_info_t sd_info;
    fat_volume_t volume;
    fat_file_t file;
    uint32_t sectors_to_load;
    uint8_t probed;
    uint8_t loaded;
} custom_app_t;

int custom_app_probe(custom_app_t *app);
int custom_app_load(custom_app_t *app);

#endif /* CUSTOM_LOADER_H */
