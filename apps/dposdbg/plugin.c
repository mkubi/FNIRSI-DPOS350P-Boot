#include <stdint.h>

#define DPOSDBG_TRACE_MAGIC       0x30435254U /* "TRC0" */
#define DPOSDBG_TRACE_VERSION     5U
#define DPOSDBG_TRACE_CAPACITY    768U

#define DPOSDBG_EVENT_FPGA_SELECTOR 0x00001000U
#define DPOSDBG_EVENT_FPGA_WRITE    0x00001001U
#define DPOSDBG_EVENT_FPGA_READ     0x00001002U
#define DPOSDBG_EVENT_CSI_DESTINATION 0x00002000U
#define JOB_FLAG_CSI_DESTINATION   (1U << 13)

#define JOB_FLAG_HAVE_69          (1U << 0)
#define JOB_FLAG_HAVE_6A          (1U << 1)
#define JOB_FLAG_HAVE_6B          (1U << 2)
#define JOB_FLAG_STARTED          (1U << 3)
#define JOB_FLAG_HAVE_64          (1U << 4)
#define JOB_FLAG_POLL65_DONE      (1U << 5)
#define JOB_FLAG_HAVE_67          (1U << 6)
#define JOB_FLAG_HAVE_66          (1U << 7)
#define JOB_FLAG_COMPLETE         (1U << 8)
#define JOB_FLAG_TRUNCATED        (1U << 9)
#define JOB_FLAG_OVERFLOW         (1U << 10)
#define JOB_FLAG_UNEXPECTED       (1U << 11)
#define JOB_FLAG_INCOMPLETE_PAYLOAD (1U << 12)

#define STOCK_F_OPEN              0x40059B8CU
#define STOCK_F_WRITE             0x4005A1D4U
#define STOCK_F_CLOSE             0x4005A694U
#define STOCK_FA_WRITE_CREATE     0x0AU
#define STOCK_FILE_OBJECT_SIZE    0x254U

/* 2025-06-19 build globals, independently cross-checked in the symbol map. */
#define STOCK_SYS_INFO            0x40544E68U
#define STOCK_RUN_STOP            (STOCK_SYS_INFO + 0x3CU)
#define STOCK_SYS_RUN_COUNT       (STOCK_SYS_INFO + 0x64CU)
#define STOCK_TOUCH_TICK          0x42E68CD8U

typedef struct {
    uint32_t sequence;
    uint32_t tick_ms;
    uint32_t run_count;
    uint32_t state_pack;
    uint32_t flags;
    uint32_t value_69;
    uint32_t value_6a;
    uint32_t value_6b;
    uint32_t poll65_count;
    uint32_t result_66;
    uint32_t status_pack;
    uint32_t counts_pack;
    uint32_t begin_call_site;
    uint32_t last_call_site;
    uint32_t selector_count;
    uint32_t primitive_count;
} dposdbg_job_record_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t capacity;
    uint32_t write_index;
    uint32_t wrapped;
    uint32_t export_count;
    uint32_t dropped;
    uint32_t record_size;
    dposdbg_job_record_t records[DPOSDBG_TRACE_CAPACITY];
} dposdbg_trace_t;

typedef int (*stock_f_open_t)(void *file, const char *path, uint32_t mode);
typedef int (*stock_f_write_t)(void *file, const void *data,
                               uint32_t bytes, void *bytes_written);
typedef int (*stock_f_close_t)(void *file);

__attribute__((section(".trace"), used, aligned(32)))
dposdbg_trace_t dposdbg_trace = {
    DPOSDBG_TRACE_MAGIC,
    DPOSDBG_TRACE_VERSION,
    DPOSDBG_TRACE_CAPACITY,
    0U,
    0U,
    0U,
    0U,
    (uint32_t)sizeof(dposdbg_job_record_t),
    {{0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U}}
};

/* Keep mutable state in the loadable image, not NOLOAD BSS. */
__attribute__((section(".data"), aligned(8)))
static uint32_t trace_file_object[STOCK_FILE_OBJECT_SIZE / sizeof(uint32_t)] = {
    0U
};

static const char trace_path[] = "/DPOSDBG.TRC";

__attribute__((section(".data"), used, aligned(4)))
static dposdbg_job_record_t current_job = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
};

__attribute__((section(".data"), used, aligned(4)))
static uint32_t current_selector = 0U;

__attribute__((section(".data"), used, aligned(4)))
static uint32_t job_active = 0U;

static uint32_t count_get(uint32_t shift)
{
    return (current_job.counts_pack >> shift) & 0xFFU;
}

static void count_set(uint32_t shift, uint32_t count)
{
    current_job.counts_pack &= ~(0xFFU << shift);
    current_job.counts_pack |= (count & 0xFFU) << shift;
}

static void append_be(uint32_t *value, uint32_t count_shift, uint32_t byte)
{
    uint32_t count = count_get(count_shift);

    if (count >= 4U) {
        current_job.flags |= JOB_FLAG_OVERFLOW;
        return;
    }

    *value = (*value << 8) | (byte & 0xFFU);
    count_set(count_shift, count + 1U);
}

static void clear_current_job(void)
{
    current_job.sequence = 0U;
    current_job.tick_ms = 0U;
    current_job.run_count = 0U;
    current_job.state_pack = 0U;
    current_job.flags = 0U;
    current_job.value_69 = 0U;
    current_job.value_6a = 0U;
    current_job.value_6b = 0U;
    current_job.poll65_count = 0U;
    current_job.result_66 = 0U;
    current_job.status_pack = 0U;
    current_job.counts_pack = 0U;
    current_job.begin_call_site = 0U;
    current_job.last_call_site = 0U;
    current_job.selector_count = 0U;
    current_job.primitive_count = 0U;
}

static void commit_current_job(uint32_t extra_flags)
{
    uint32_t index;
    uint32_t slot;
    dposdbg_job_record_t *record;

    if (job_active == 0U)
        return;

    current_job.flags |= extra_flags;
    if ((current_job.flags & (JOB_FLAG_HAVE_69 | JOB_FLAG_HAVE_6A |
                              JOB_FLAG_HAVE_6B | JOB_FLAG_STARTED |
                              JOB_FLAG_HAVE_64 | JOB_FLAG_POLL65_DONE |
                              JOB_FLAG_HAVE_67 | JOB_FLAG_HAVE_66)) ==
        (JOB_FLAG_HAVE_69 | JOB_FLAG_HAVE_6A | JOB_FLAG_HAVE_6B |
         JOB_FLAG_STARTED | JOB_FLAG_HAVE_64 | JOB_FLAG_POLL65_DONE |
         JOB_FLAG_HAVE_67 | JOB_FLAG_HAVE_66) &&
        count_get(0U) == 2U && count_get(8U) == 1U &&
        count_get(16U) == 4U && count_get(24U) == 4U) {
        current_job.flags |= JOB_FLAG_COMPLETE;
    } else {
        current_job.flags |= JOB_FLAG_INCOMPLETE_PAYLOAD;
    }

    index = dposdbg_trace.write_index;
    slot = index % DPOSDBG_TRACE_CAPACITY;
    if (index >= DPOSDBG_TRACE_CAPACITY)
        dposdbg_trace.wrapped = 1U;

    current_job.sequence = index;
    record = &dposdbg_trace.records[slot];
    *record = current_job;
    dposdbg_trace.write_index = index + 1U;

    clear_current_job();
    job_active = 0U;
}

static uint32_t read_u32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static uint32_t read_u8(uint32_t address)
{
    return *(volatile uint8_t *)(uintptr_t)address;
}

static void begin_job(uint32_t call_site)
{
    if (job_active != 0U)
        commit_current_job(JOB_FLAG_TRUNCATED);

    clear_current_job();
    current_job.tick_ms = read_u32(STOCK_TOUCH_TICK);
    current_job.run_count = read_u32(STOCK_SYS_RUN_COUNT);
    current_job.state_pack = read_u8(STOCK_RUN_STOP) & 0xFFU;
    current_job.begin_call_site = call_site;
    current_job.last_call_site = call_site;
    current_job.selector_count = 1U;
    current_job.primitive_count = 1U;
    job_active = 1U;
}

static void handle_selector(uint32_t selector, uint32_t call_site)
{
    selector &= 0xFFU;

    if (selector == 0x69U) {
        begin_job(call_site);
    } else if (job_active != 0U && current_selector == 0x66U &&
               count_get(24U) != 0U) {
        commit_current_job(JOB_FLAG_TRUNCATED);
    }

    if (job_active != 0U) {
        if (selector == 0x6AU)
            current_job.flags |= JOB_FLAG_HAVE_6A;
        current_job.last_call_site = call_site;
        if (selector != 0x69U) {
            current_job.selector_count += 1U;
            current_job.primitive_count += 1U;
        }
    }

    current_selector = selector;
}

static void handle_write(uint32_t byte, uint32_t call_site)
{
    byte &= 0xFFU;

    if (job_active == 0U)
        return;

    current_job.last_call_site = call_site;
    current_job.primitive_count += 1U;

    switch (current_selector) {
    case 0x69U:
        append_be(&current_job.value_69, 0U, byte);
        current_job.flags |= JOB_FLAG_HAVE_69;
        break;
    case 0x6AU:
        append_be(&current_job.value_6a, 8U, byte);
        current_job.flags |= JOB_FLAG_HAVE_6A;
        break;
    case 0x6BU:
        append_be(&current_job.value_6b, 16U, byte);
        current_job.flags |= JOB_FLAG_HAVE_6B;
        break;
    case 0x68U:
        if (byte == 1U)
            current_job.flags |= JOB_FLAG_STARTED;
        break;
    default:
        current_job.flags |= JOB_FLAG_UNEXPECTED;
        break;
    }
}

static void handle_read(uint32_t byte, uint32_t call_site)
{
    uint32_t count;

    byte &= 0xFFU;

    if (job_active == 0U)
        return;

    current_job.last_call_site = call_site;
    current_job.primitive_count += 1U;

    switch (current_selector) {
    case 0x64U:
        current_job.status_pack &= ~0xFFU;
        current_job.status_pack |= byte;
        current_job.flags |= JOB_FLAG_HAVE_64;
        break;
    case 0x65U:
        if ((byte & 1U) == 0U) {
            current_job.poll65_count += 1U;
        } else {
            current_job.status_pack &= ~(0xFFU << 8);
            current_job.status_pack |= byte << 8;
            current_job.flags |= JOB_FLAG_POLL65_DONE;
        }
        break;
    case 0x67U:
        current_job.status_pack &= ~(0xFFU << 16);
        current_job.status_pack |= byte << 16;
        current_job.flags |= JOB_FLAG_HAVE_67;
        break;
    case 0x66U:
        append_be(&current_job.result_66, 24U, byte);
        current_job.flags |= JOB_FLAG_HAVE_66;
        count = count_get(24U);
        if (count == 4U)
            commit_current_job(0U);
        break;
    default:
        current_job.flags |= JOB_FLAG_UNEXPECTED;
        break;
    }
}

void dposdbg_trace_primitive(uint32_t event, uint32_t value,
                             uint32_t call_site)
{
    if (event == DPOSDBG_EVENT_CSI_DESTINATION) {
        uint32_t index = dposdbg_trace.write_index;
        uint32_t slot = index % DPOSDBG_TRACE_CAPACITY;
        dposdbg_job_record_t *record = &dposdbg_trace.records[slot];
        if (index >= DPOSDBG_TRACE_CAPACITY)
            dposdbg_trace.wrapped = 1U;
        clear_current_job();
        current_job.sequence = index;
        current_job.tick_ms = read_u32(STOCK_TOUCH_TICK);
        current_job.run_count = read_u32(STOCK_SYS_RUN_COUNT);
        current_job.state_pack = read_u8(STOCK_RUN_STOP) & 0xFFU;
        current_job.flags = JOB_FLAG_CSI_DESTINATION;
        current_job.value_6b = value;
        current_job.begin_call_site = call_site;
        current_job.last_call_site = call_site;
        current_job.primitive_count = 1U;
        *record = current_job;
        dposdbg_trace.write_index = index + 1U;
        clear_current_job();
        job_active = 0U;
        return;
    }
    if (event == DPOSDBG_EVENT_FPGA_SELECTOR)
        handle_selector(value, call_site);
    else if (event == DPOSDBG_EVENT_FPGA_WRITE)
        handle_write(value, call_site);
    else if (event == DPOSDBG_EVENT_FPGA_READ)
        handle_read(value, call_site);
}

static int trace_export(void)
{
    stock_f_open_t stock_f_open =
        (stock_f_open_t)(uintptr_t)STOCK_F_OPEN;
    stock_f_write_t stock_f_write =
        (stock_f_write_t)(uintptr_t)STOCK_F_WRITE;
    stock_f_close_t stock_f_close =
        (stock_f_close_t)(uintptr_t)STOCK_F_CLOSE;
    int result;
    int close_result;

    result = stock_f_open(trace_file_object, trace_path,
                          STOCK_FA_WRITE_CREATE);
    if (result != 0)
        return result;

    result = stock_f_write(trace_file_object, &dposdbg_trace,
                           (uint32_t)sizeof(dposdbg_trace), 0);
    close_result = stock_f_close(trace_file_object);

    if (result != 0)
        return result;
    return close_result;
}

void dposdbg_after_screenshot(void)
{
    /* Preserve a partial in-flight transaction in the exported snapshot. */
    if (job_active != 0U)
        commit_current_job(JOB_FLAG_TRUNCATED);

    dposdbg_trace.export_count += 1U;
    (void)trace_export();
}
