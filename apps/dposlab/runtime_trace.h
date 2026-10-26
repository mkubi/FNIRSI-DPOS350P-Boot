#ifndef DPOSLAB_RUNTIME_TRACE_H
#define DPOSLAB_RUNTIME_TRACE_H

#include <stdint.h>
#include "runtime_snapshot.h"

#define DPOSLAB_TRACE_MAGIC       0x42414C44U /* "DLAB" */
#define DPOSLAB_TRACE_VERSION     1U
#define DPOSLAB_TRACE_CAPACITY    32U

enum dposlab_checkpoint {
    DPOSLAB_CP_NONE              = 0U,
    DPOSLAB_CP_FPGA_START_ENTRY  = 1U,
    DPOSLAB_CP_FPGA_START_RETURN = 2U,

    /*
     * Reserved for future internal FPGA_Start checkpoints.
     */
    DPOSLAB_CP_FPGA_PHASE_A      = 0x10U,
    DPOSLAB_CP_FPGA_PHASE_B      = 0x11U,
    DPOSLAB_CP_FPGA_CLOCK_95M    = 0x12U,
    DPOSLAB_CP_FPGA_CLOCK_10M    = 0x13U,
    DPOSLAB_CP_FPGA_PHASE_C      = 0x14U
};

struct dposlab_trace_record {
    uint32_t sequence;
    uint32_t checkpoint;
    uint32_t execution_address;
    uint32_t reserved;

    struct dposlab_runtime_snapshot snapshot;
};

struct dposlab_trace_buffer {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t record_size;

    uint32_t capacity;
    uint32_t record_count;
    uint32_t dropped;
    uint32_t flags;

    struct dposlab_trace_record record[DPOSLAB_TRACE_CAPACITY];
};

extern volatile struct dposlab_trace_buffer dposlab_trace;

void dposlab_trace_reset(void);

void dposlab_trace_append(
    uint32_t checkpoint,
    uint32_t execution_address,
    const volatile struct dposlab_runtime_snapshot *snapshot);

const volatile struct dposlab_trace_record *
dposlab_trace_first(void);

const volatile struct dposlab_trace_record *
dposlab_trace_last(void);

#endif
