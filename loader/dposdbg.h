#ifndef DPOSDBG_LOADER_H
#define DPOSDBG_LOADER_H

#include <stdint.h>

#define DPOSDBG_LOAD_ADDRESS       0x42E70000U
#define DPOSDBG_MAX_SIZE           0x00010000U
#define DPOSDBG_RESIDENT_LIMIT     0x42E80000U
#define DPOSDBG_MAGIC              0x30474244U /* "DBG0" */
#define DPOSDBG_ABI_VERSION        2U
#define DPOSDBG_MAX_HOOKS          64U
#define DPOSDBG_VENEER_MIN         0x40055AD4U
#define DPOSDBG_VENEER_LIMIT       0x40055B54U

#define DPOSDBG_PATCH_ARM_B        1U
#define DPOSDBG_PATCH_ARM_BL       2U

#define DPOSDBG_DIAG_IDLE             0U
#define DPOSDBG_DIAG_SD_INIT          1U
#define DPOSDBG_DIAG_FAT_MOUNT        2U
#define DPOSDBG_DIAG_FILE_LOOKUP      3U
#define DPOSDBG_DIAG_IMAGE_LOAD       4U
#define DPOSDBG_DIAG_HEADER_VALIDATE  5U
#define DPOSDBG_DIAG_MANIFEST_VALIDATE 6U
#define DPOSDBG_DIAG_STOCK_VALIDATE   7U
#define DPOSDBG_DIAG_PATCH_WRITE      8U
#define DPOSDBG_DIAG_CACHE_MAINT      9U
#define DPOSDBG_DIAG_READBACK        10U
#define DPOSDBG_DIAG_SUCCESS         11U

typedef struct {
    uint32_t magic;
    uint32_t abi_version;
    uint32_t image_size;
    uint32_t trace_address;
    uint32_t hook_count;
    uint32_t hook_table_offset;
    uint32_t flags;
    uint32_t reserved;
} dposdbg_header_t;

typedef struct {
    uint32_t site;
    uint32_t expected_word;
    uint32_t veneer_address;
    uint32_t target_address;
    uint32_t patch_type;
    uint32_t flags;
} dposdbg_hook_t;

typedef struct {
    uint32_t phase;
    int32_t result;
    uint32_t file_size;
    uint32_t sector_index;
    uint32_t abi_version;
    uint32_t image_size;
    uint32_t hook_count;
    uint32_t hook_table_offset;
    uint32_t hook_index;
    uint32_t patch_type;
    uint32_t site;
    uint32_t expected_word;
    uint32_t actual_word;
    uint32_t veneer_address;
    uint32_t target_address;
    uint32_t generated_word;
    uint32_t veneer_word;
    uint32_t veneer_target;
} dposdbg_diag_t;

int dposdbg_load_and_patch(void);
const dposdbg_diag_t *dposdbg_last_diag(void);
const char *dposdbg_diag_phase_name(uint32_t phase);
const char *dposdbg_patch_type_name(uint32_t patch_type);

#endif
