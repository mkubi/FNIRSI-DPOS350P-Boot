#ifndef DEBUG_EXE2_LOADER_H
#define DEBUG_EXE2_LOADER_H

#include <stdint.h>

#include "fat.h"
#include "loader.h"
#include "sdmmc0.h"

#define DEBUG_EXE2_LOAD_ADDRESS  0x40000000U
#define DEBUG_EXE2_ENTRY_ADDRESS 0x40000000U
#define DEBUG_EXE2_EXPECTED_SIZE 0x000957E0U

/* Stage 2 sentinel used to test RAM persistence across stock EXE2 startup. */
#define DEBUG_RESIDENT_TEST_ADDRESS 0x42E70000U
#define DEBUG_RESIDENT_TEST_SIZE    64U

/* FAT 8.3 representation of EXE2DBG.BIN. */
#define DEBUG_EXE2_NAME_83 \
    { 'E','X','E','2','D','B','G',' ','B','I','N' }

typedef struct {
    loader_image_t image;
    sdmmc0_info_t sd_info;
    fat_volume_t volume;
    fat_file_t file;
    uint32_t sectors_to_load;
    uint8_t probed;
    uint8_t loaded;
} debug_exe2_app_t;

int debug_exe2_probe(debug_exe2_app_t *app);
int debug_exe2_load(debug_exe2_app_t *app);

#endif /* DEBUG_EXE2_LOADER_H */
