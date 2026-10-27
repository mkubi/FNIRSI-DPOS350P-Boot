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
     * Internal FPGA_Start checkpoints.
     */
    DPOSLAB_CP_FPGA_EARLY_INIT   = 0x0FU,
    DPOSLAB_CP_FPGA_PHASE_A      = 0x10U,
    DPOSLAB_CP_FPGA_PHASE_B      = 0x11U,
    DPOSLAB_CP_FPGA_CLOCK_95M    = 0x12U,
    DPOSLAB_CP_FPGA_CLOCK_10M    = 0x13U,
    DPOSLAB_CP_FPGA_PHASE_C      = 0x14U,

    DPOSLAB_CP_CSI_AFTER_CLOCK_RESET = 0x20U,
    DPOSLAB_CP_CSI_AFTER_BASE_BIND   = 0x21U,
    DPOSLAB_CP_CSI_AFTER_ENABLE_A    = 0x22U,
    DPOSLAB_CP_CSI_AFTER_ENABLE_B    = 0x23U,
    DPOSLAB_CP_CSI_AFTER_CONFIG_8000 = 0x24U,
    DPOSLAB_CP_CSI_AFTER_PINMUX      = 0x25U,

    /* Stock asynchronous job transaction checkpoints. */
    DPOSLAB_CP_SMEC_AFTER_ECHO68     = 0x30U,
    DPOSLAB_CP_SMEC_AFTER_VALID67    = 0x31U,
    DPOSLAB_CP_SMEC_JOB2_BEFORE_START = 0x32U,
    DPOSLAB_CP_SMEC_JOB2_AFTER_START  = 0x33U,
    DPOSLAB_CP_SMEC_JOB2_BEFORE_POLL65 = 0x34U,
    DPOSLAB_CP_SMEC_JOB2_AFTER_POLL65  = 0x35U
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
