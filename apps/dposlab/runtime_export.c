#include <stdint.h>
#include "runtime_export.h"
#include "runtime_trace.h"

#define STOCK_F_OPEN              0x40059B8CU
#define STOCK_F_WRITE             0x4005A1D4U
#define STOCK_F_CLOSE             0x4005A694U

#define STOCK_FA_WRITE_CREATE     0x0AU
#define STOCK_FILE_OBJECT_SIZE    0x254U

#define DPOSLAB_EXPORT_OK          0
#define DPOSLAB_EXPORT_BAD_MAGIC  -1
#define DPOSLAB_EXPORT_BAD_VER    -2
#define DPOSLAB_EXPORT_BAD_HEADER -3
#define DPOSLAB_EXPORT_BAD_RECORD -4
#define DPOSLAB_EXPORT_BAD_CAP    -5
#define DPOSLAB_EXPORT_BAD_COUNT  -6
#define DPOSLAB_EXPORT_OVERFLOW   -7
#define DPOSLAB_EXPORT_SHORT      -8

typedef int (*stock_f_open_t)(void *file,
                              const char *path,
                              uint32_t mode);

typedef int (*stock_f_write_t)(void *file,
                               const void *data,
                               uint32_t bytes,
                               void *bytes_written);

typedef int (*stock_f_close_t)(void *file);

/*
 * Keep the FatFs-compatible file object resident and suitably aligned.
 * The stock object size is inherited from the hardware-confirmed
 * DPOSDBG export implementation.
 */
__attribute__((section(".data"), used, aligned(8)))
static uint32_t trace_file_object[
    STOCK_FILE_OBJECT_SIZE / sizeof(uint32_t)
] = { 0U };

static const char trace_path[] = "/DPOSLAB.TRC";

volatile struct dposlab_export_status dposlab_export_status;

static void clear_file_object(void)
{
    uint32_t i;

    for (i = 0U;
         i < STOCK_FILE_OBJECT_SIZE / sizeof(uint32_t);
         ++i)
        trace_file_object[i] = 0U;
}

static int validate_trace(uint32_t *export_size)
{
    uint32_t records_bytes;
    uint32_t size;

    if (dposlab_trace.magic != DPOSLAB_TRACE_MAGIC)
        return DPOSLAB_EXPORT_BAD_MAGIC;

    if (dposlab_trace.version != DPOSLAB_TRACE_VERSION)
        return DPOSLAB_EXPORT_BAD_VER;

    if (dposlab_trace.header_size !=
        (uint32_t)(uintptr_t)&dposlab_trace.record[0] -
        (uint32_t)(uintptr_t)&dposlab_trace)
        return DPOSLAB_EXPORT_BAD_HEADER;

    if (dposlab_trace.record_size !=
        (uint32_t)sizeof(struct dposlab_trace_record))
        return DPOSLAB_EXPORT_BAD_RECORD;

    if (dposlab_trace.capacity == 0U ||
        dposlab_trace.capacity > DPOSLAB_TRACE_CAPACITY)
        return DPOSLAB_EXPORT_BAD_CAP;

    if (dposlab_trace.record_count > dposlab_trace.capacity)
        return DPOSLAB_EXPORT_BAD_COUNT;

    records_bytes =
        dposlab_trace.record_count * dposlab_trace.record_size;

    if (dposlab_trace.record_count != 0U &&
        records_bytes / dposlab_trace.record_count !=
            dposlab_trace.record_size)
        return DPOSLAB_EXPORT_OVERFLOW;

    size = dposlab_trace.header_size + records_bytes;

    if (size < dposlab_trace.header_size ||
        size > (uint32_t)sizeof(dposlab_trace))
        return DPOSLAB_EXPORT_OVERFLOW;

    *export_size = size;
    return DPOSLAB_EXPORT_OK;
}

void dposlab_trace_export_after_screenshot(void)
{
    stock_f_open_t stock_f_open =
        (stock_f_open_t)(uintptr_t)STOCK_F_OPEN;

    stock_f_write_t stock_f_write =
        (stock_f_write_t)(uintptr_t)STOCK_F_WRITE;

    stock_f_close_t stock_f_close =
        (stock_f_close_t)(uintptr_t)STOCK_F_CLOSE;

    uint32_t export_size = 0U;
    uint32_t bytes_written = 0U;
    int validation_result;
    int open_result;
    int write_result;
    int close_result;

    dposlab_export_status.attempts += 1U;
    dposlab_export_status.bytes_requested = 0U;
    dposlab_export_status.bytes_written = 0U;
    dposlab_export_status.open_result = -1;
    dposlab_export_status.write_result = -1;
    dposlab_export_status.close_result = -1;

    validation_result = validate_trace(&export_size);
    dposlab_export_status.validation_result = validation_result;

    if (validation_result != DPOSLAB_EXPORT_OK)
        return;

    dposlab_export_status.bytes_requested = export_size;

    clear_file_object();

    /*
     * Ensure all trace records are committed before the stock filesystem
     * code observes the buffer.
     */
    __asm__ volatile ("dmb sy" ::: "memory");

    open_result = stock_f_open(
        trace_file_object,
        trace_path,
        STOCK_FA_WRITE_CREATE);

    dposlab_export_status.open_result = open_result;

    if (open_result != 0)
        return;

    write_result = stock_f_write(
        trace_file_object,
        (const void *)&dposlab_trace,
        export_size,
        &bytes_written);

    dposlab_export_status.write_result = write_result;
    dposlab_export_status.bytes_written = bytes_written;

    close_result = stock_f_close(trace_file_object);
    dposlab_export_status.close_result = close_result;

    if (write_result != 0 || close_result != 0)
        return;

    if (bytes_written != export_size) {
        dposlab_export_status.write_result =
            DPOSLAB_EXPORT_SHORT;
        return;
    }

    dposlab_export_status.successes += 1U;
}
