#include <stdint.h>
#include "runtime_trace.h"

volatile struct dposlab_trace_buffer dposlab_trace;

static void copy_snapshot(
    volatile struct dposlab_runtime_snapshot *destination,
    const volatile struct dposlab_runtime_snapshot *source)
{
    volatile uint32_t *out;
    const volatile uint32_t *in;
    uint32_t words;
    uint32_t i;

    out = (volatile uint32_t *)(void *)destination;
    in = (const volatile uint32_t *)(const void *)source;
    words = (uint32_t)(sizeof(*destination) / sizeof(uint32_t));

    for (i = 0U; i < words; ++i)
        out[i] = in[i];
}

void dposlab_trace_reset(void)
{
    uint32_t i;
    uint32_t words;
    volatile uint32_t *raw;

    raw = (volatile uint32_t *)(void *)&dposlab_trace;
    words = (uint32_t)(sizeof(dposlab_trace) / sizeof(uint32_t));

    for (i = 0U; i < words; ++i)
        raw[i] = 0U;

    dposlab_trace.magic = DPOSLAB_TRACE_MAGIC;
    dposlab_trace.version = DPOSLAB_TRACE_VERSION;
    dposlab_trace.header_size =
        (uint32_t)(uintptr_t)&dposlab_trace.record[0] -
        (uint32_t)(uintptr_t)&dposlab_trace;
    dposlab_trace.record_size =
        (uint32_t)sizeof(struct dposlab_trace_record);
    dposlab_trace.capacity = DPOSLAB_TRACE_CAPACITY;
}

void dposlab_trace_append(
    uint32_t checkpoint,
    uint32_t execution_address,
    const volatile struct dposlab_runtime_snapshot *snapshot)
{
    volatile struct dposlab_trace_record *record;
    uint32_t index;

    if (dposlab_trace.magic != DPOSLAB_TRACE_MAGIC ||
        dposlab_trace.version != DPOSLAB_TRACE_VERSION)
        dposlab_trace_reset();

    index = dposlab_trace.record_count;

    if (index >= dposlab_trace.capacity) {
        dposlab_trace.dropped += 1U;
        return;
    }

    record = &dposlab_trace.record[index];

    record->sequence = index;
    record->checkpoint = checkpoint;
    record->execution_address = execution_address;
    record->reserved = 0U;

    copy_snapshot(&record->snapshot, snapshot);

    /*
     * Publish the record only after all fields have been written.
     */
    __asm__ volatile ("dmb sy" ::: "memory");
    dposlab_trace.record_count = index + 1U;
}

const volatile struct dposlab_trace_record *
dposlab_trace_first(void)
{
    if (dposlab_trace.record_count == 0U)
        return 0;

    return &dposlab_trace.record[0];
}

const volatile struct dposlab_trace_record *
dposlab_trace_last(void)
{
    uint32_t count = dposlab_trace.record_count;

    if (count == 0U)
        return 0;

    if (count > dposlab_trace.capacity)
        count = dposlab_trace.capacity;

    return &dposlab_trace.record[count - 1U];
}
