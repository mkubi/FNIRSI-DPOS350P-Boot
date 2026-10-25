#ifndef DPOSDBG_LOADER_H
#define DPOSDBG_LOADER_H

#include <stdint.h>

#define DPOSDBG_LOAD_ADDRESS       0x42E70000U
#define DPOSDBG_MAX_SIZE           0x00010000U
#define DPOSDBG_RESIDENT_LIMIT     0x42E80000U
#define DPOSDBG_MAGIC              0x30474244U /* "DBG0" */
#define DPOSDBG_ABI_VERSION        1U
#define DPOSDBG_SCREENSHOT_HOOK    0x400350F0U
#define DPOSDBG_VENEER_ADDRESS     0x40055AD4U
#define DPOSDBG_SELECTOR_VENEER    0x40055ADCU
#define DPOSDBG_WRITE_VENEER       0x40055AE4U
#define DPOSDBG_READ_VENEER        0x40055AECU
#define DPOSDBG_STOCK_SCREENSHOT   0x40053388U

typedef struct {
    uint32_t magic;
    uint32_t abi_version;
    uint32_t image_size;
    uint32_t screenshot_hook;
    uint32_t trace_address;
    uint32_t selector_wrapper;
    uint32_t write_wrapper;
    uint32_t read_wrapper;
} dposdbg_header_t;

int dposdbg_load_and_patch(void);

#endif /* DPOSDBG_LOADER_H */
