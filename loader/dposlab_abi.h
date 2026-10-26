#ifndef DPOSLAB_ABI_H
#define DPOSLAB_ABI_H

#include <stdint.h>

#define DPOSLAB_HANDOFF_MAGIC   0x3342414CU /* "LAB3" */
#define DPOSLAB_HANDOFF_VERSION 1U

#define DPOSLAB_FLAG_EXE2_LOADED (1U << 0)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t flags;

    uint32_t stage2_sp;

    uint32_t exe2_base;
    uint32_t exe2_size;
    uint32_t exe2_entry;

    uint32_t dposlab_base;
    uint32_t dposlab_size;
    uint32_t dposlab_entry;
} dposlab_handoff_t;

#endif /* DPOSLAB_ABI_H */
