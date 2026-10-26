#ifndef DPOSLAB_LOADER_H
#define DPOSLAB_LOADER_H
#include "debug_exe2.h"
#include "dposlab_abi.h"
#include "loader.h"
#define DPOSLAB_LOAD_ADDRESS  0x42E70000U
#define DPOSLAB_LIMIT_ADDRESS 0x42E80000U
#define DPOSLAB_MAX_SIZE      0x0000E000U
#define DPOSLAB_NAME_83 { 'D','P','O','S','L','A','B',' ','B','I','N' }
int dposlab_load(debug_exe2_app_t *exe2, loader_image_t *image);
void dposlab_start(const loader_image_t *image,
                   dposlab_handoff_t *handoff)
    __attribute__((noreturn));
#endif
