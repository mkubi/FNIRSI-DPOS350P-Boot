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

#define INIT_CHECK_MAGIC            0x4B484343U /* "CCHK" */
#define INIT_CHECK_VERSION          1U
#define INIT_CHECK_COUNT            14U
#define INIT_CHECK_SAMPLES          1000000U
#define PE_CFG0_ADDRESS             0x01C20890U
#define PE_CFG1_ADDRESS             0x01C20894U
#define PE_DATA_ADDRESS             0x01C208A0U
#define PE_SAMPLE_MASK              0x0000FF0DU

typedef struct {
    uint32_t captured, cfg0, cfg1, first, last;
    uint32_t changed_mask, high_mask, low_mask, samples;
} dposdbg_init_checkpoint_t;

typedef struct {
    uint32_t magic, version, checkpoint_count, sample_mask;
    dposdbg_init_checkpoint_t checkpoint[INIT_CHECK_COUNT];
} dposdbg_init_check_snapshot_t;

__attribute__((section(".data"), used, aligned(4)))
static dposdbg_init_check_snapshot_t init_check_snapshot = {
    INIT_CHECK_MAGIC, INIT_CHECK_VERSION, INIT_CHECK_COUNT, PE_SAMPLE_MASK,
    {{0U}}
};

void dposdbg_capture_checkpoint(uint32_t index)
{
    dposdbg_init_checkpoint_t *out;
    volatile const uint32_t *pe_data = (volatile const uint32_t *)(uintptr_t)PE_DATA_ADDRESS;
    uint32_t previous, current, i;
    if (index >= INIT_CHECK_COUNT) return;
    out = &init_check_snapshot.checkpoint[index];
    out->cfg0 = *(volatile const uint32_t *)(uintptr_t)PE_CFG0_ADDRESS;
    out->cfg1 = *(volatile const uint32_t *)(uintptr_t)PE_CFG1_ADDRESS;
    previous = *pe_data & PE_SAMPLE_MASK;
    out->first = previous; out->changed_mask = 0U; out->high_mask = previous;
    out->low_mask = PE_SAMPLE_MASK & ~previous; out->samples = INIT_CHECK_SAMPLES;
    for (i = 0U; i < INIT_CHECK_SAMPLES; ++i) {
        current = *pe_data & PE_SAMPLE_MASK;
        out->changed_mask |= previous ^ current;
        out->high_mask |= current;
        out->low_mask |= PE_SAMPLE_MASK & ~current;
        previous = current;
    }
    out->last = previous;
    __asm__ volatile ("dmb ish" ::: "memory");
    out->captured = 1U;
}

#define CSI_BUFFER_MAGIC           0x46554243U /* "CBUF" */
#define CSI_BUFFER_VERSION         1U
#define CSI_BUFFER_BASE            0x01CB4000U
#define CSI_BUFFER_SIZE            0x00002134U
#define CSI_BUFFER_BLOCK_SIZE      256U
#define CSI_BUFFER_BLOCKS          34U
#define CSI_BUFFER_EDGE_BYTES      64U
#define CSI_BUFFER_FNV_OFFSET      0x811C9DC5U
#define CSI_BUFFER_FNV_PRIME       0x01000193U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t captured_before;
    uint32_t captured_after;
    uint32_t destination_register;
    uint32_t destination_address;
    uint32_t buffer_size;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t hash_before;
    uint32_t hash_after;
    uint32_t changed_block_count;
    uint32_t first_changed_block;
    uint32_t last_changed_block;
    uint32_t csi_status_before;
    uint32_t csi_status_after;
    uint8_t prefix_before[CSI_BUFFER_EDGE_BYTES];
    uint8_t prefix_after[CSI_BUFFER_EDGE_BYTES];
    uint8_t suffix_before[CSI_BUFFER_EDGE_BYTES];
    uint8_t suffix_after[CSI_BUFFER_EDGE_BYTES];
    uint32_t block_hash_before[CSI_BUFFER_BLOCKS];
    uint32_t block_hash_after[CSI_BUFFER_BLOCKS];
} dposdbg_csi_buffer_t;

__attribute__((section(".data"), used, aligned(32)))
static dposdbg_csi_buffer_t csi_buffer = {
    CSI_BUFFER_MAGIC, CSI_BUFFER_VERSION, 0U, 0U,
    0U, 0U, CSI_BUFFER_SIZE, CSI_BUFFER_BLOCK_SIZE, CSI_BUFFER_BLOCKS,
    0U, 0U, 0U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0U, 0U,
    {0U}, {0U}, {0U}, {0U}, {0U}, {0U}
};

static void csi_buffer_invalidate(uint32_t address, uint32_t size)
{
    uint32_t line;
    uint32_t end;
    uint32_t zero = 0U;

    line = address & ~31U;
    end = (address + size + 31U) & ~31U;
    while (line < end) {
        __asm__ volatile ("mcr p15, 0, %0, c7, c6, 1" :: "r" (line) : "memory");
        line += 32U;
    }
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" :: "r" (zero) : "memory");
    __asm__ volatile ("isb" ::: "memory");
}

static uint32_t csi_buffer_hash(const volatile uint8_t *data, uint32_t size)
{
    uint32_t hash = CSI_BUFFER_FNV_OFFSET;
    uint32_t i;
    for (i = 0U; i < size; ++i) {
        hash ^= data[i];
        hash *= CSI_BUFFER_FNV_PRIME;
    }
    return hash;
}

static void csi_buffer_copy(uint8_t *destination,
                            const volatile uint8_t *source,
                            uint32_t size)
{
    uint32_t i;
    for (i = 0U; i < size; ++i)
        destination[i] = source[i];
}

static void csi_buffer_capture_before(void)
{
    volatile const uint8_t *data;
    uint32_t destination_register;
    uint32_t address;
    uint32_t block;
    uint32_t offset;
    uint32_t size;

    if (csi_buffer.captured_before != 0U)
        return;

    destination_register = *(volatile const uint32_t *)(uintptr_t)(CSI_BUFFER_BASE + 0x50U);
    address = destination_register << 2;
    csi_buffer.destination_register = destination_register;
    csi_buffer.destination_address = address;
    csi_buffer.csi_status_before =
        *(volatile const uint32_t *)(uintptr_t)(CSI_BUFFER_BASE + 0x74U);

    csi_buffer_invalidate(address, CSI_BUFFER_SIZE);
    data = (volatile const uint8_t *)(uintptr_t)address;
    csi_buffer.hash_before = csi_buffer_hash(data, CSI_BUFFER_SIZE);

    for (block = 0U; block < CSI_BUFFER_BLOCKS; ++block) {
        offset = block * CSI_BUFFER_BLOCK_SIZE;
        size = CSI_BUFFER_SIZE - offset;
        if (size > CSI_BUFFER_BLOCK_SIZE)
            size = CSI_BUFFER_BLOCK_SIZE;
        csi_buffer.block_hash_before[block] =
            csi_buffer_hash(data + offset, size);
    }

    csi_buffer_copy(csi_buffer.prefix_before, data, CSI_BUFFER_EDGE_BYTES);
    csi_buffer_copy(csi_buffer.suffix_before,
                    data + CSI_BUFFER_SIZE - CSI_BUFFER_EDGE_BYTES,
                    CSI_BUFFER_EDGE_BYTES);
    __asm__ volatile ("dmb ish" ::: "memory");
    csi_buffer.captured_before = 1U;
}

static void csi_buffer_capture_after(void)
{
    volatile const uint8_t *data;
    uint32_t address;
    uint32_t block;
    uint32_t offset;
    uint32_t size;
    uint32_t hash;

    if (csi_buffer.captured_before == 0U ||
        csi_buffer.captured_after != 0U)
        return;

    address = csi_buffer.destination_address;
    csi_buffer_invalidate(address, CSI_BUFFER_SIZE);
    data = (volatile const uint8_t *)(uintptr_t)address;
    csi_buffer.hash_after = csi_buffer_hash(data, CSI_BUFFER_SIZE);
    csi_buffer.changed_block_count = 0U;
    csi_buffer.first_changed_block = 0xFFFFFFFFU;
    csi_buffer.last_changed_block = 0xFFFFFFFFU;

    for (block = 0U; block < CSI_BUFFER_BLOCKS; ++block) {
        offset = block * CSI_BUFFER_BLOCK_SIZE;
        size = CSI_BUFFER_SIZE - offset;
        if (size > CSI_BUFFER_BLOCK_SIZE)
            size = CSI_BUFFER_BLOCK_SIZE;
        hash = csi_buffer_hash(data + offset, size);
        csi_buffer.block_hash_after[block] = hash;
        if (hash != csi_buffer.block_hash_before[block]) {
            if (csi_buffer.first_changed_block == 0xFFFFFFFFU)
                csi_buffer.first_changed_block = block;
            csi_buffer.last_changed_block = block;
            csi_buffer.changed_block_count += 1U;
        }
    }

    csi_buffer_copy(csi_buffer.prefix_after, data, CSI_BUFFER_EDGE_BYTES);
    csi_buffer_copy(csi_buffer.suffix_after,
                    data + CSI_BUFFER_SIZE - CSI_BUFFER_EDGE_BYTES,
                    CSI_BUFFER_EDGE_BYTES);
    csi_buffer.csi_status_after =
        *(volatile const uint32_t *)(uintptr_t)(CSI_BUFFER_BASE + 0x74U);
    __asm__ volatile ("dmb ish" ::: "memory");
    csi_buffer.captured_after = 1U;
}

#define CSI_WINDOW_MAGIC          0x57495343U /* "CSIW" */
#define CSI_WINDOW_VERSION        1U
#define CSI_WINDOW_PHASES         3U
#define CSI_WINDOW_SAMPLES        4096U
#define CSI_PE_MASK               0x0000FF0FU
#define CSI_PE_CFG0               0x01C20890U
#define CSI_PE_CFG1               0x01C20894U
#define CSI_PE_DATA               0x01C208A0U
#define CSI_BASE                  0x01CB4000U

typedef struct {
    uint32_t captured, pe_cfg0, pe_cfg1, pe_first, pe_last;
    uint32_t pe_changed, pe_high, pe_low, samples;
    uint32_t csi_if_cfg, csi_mode, csi_hsize, csi_destination;
    uint32_t csi_irq_enable, csi_status, csi_geo0, csi_geo1, csi_geo2;
} dposdbg_csi_phase_t;

typedef struct {
    uint32_t magic, version, completed, phase_count;
    uint32_t sample_mask, activity_samples, reserved0, reserved1;
    dposdbg_csi_phase_t phase[CSI_WINDOW_PHASES];
} dposdbg_csi_window_t;

__attribute__((section(".data"), used, aligned(4)))
static dposdbg_csi_window_t csi_window = {
    CSI_WINDOW_MAGIC, CSI_WINDOW_VERSION, 0U, CSI_WINDOW_PHASES,
    CSI_PE_MASK, CSI_WINDOW_SAMPLES, 0U, 0U, {{0U}}
};

void dposdbg_capture_csi_phase(uint32_t index)
{
    dposdbg_csi_phase_t *out;
    volatile const uint32_t *pe=(volatile const uint32_t *)(uintptr_t)CSI_PE_DATA;
    uint32_t previous,current,i;
    if(index>=CSI_WINDOW_PHASES || csi_window.phase[index].captured) return;
    out=&csi_window.phase[index];
    out->pe_cfg0=*(volatile const uint32_t *)(uintptr_t)CSI_PE_CFG0;
    out->pe_cfg1=*(volatile const uint32_t *)(uintptr_t)CSI_PE_CFG1;
    previous=*pe & CSI_PE_MASK; out->pe_first=previous; out->pe_changed=0U;
    out->pe_high=previous; out->pe_low=CSI_PE_MASK & ~previous; out->samples=CSI_WINDOW_SAMPLES;
    for(i=0U;i<CSI_WINDOW_SAMPLES;++i){current=*pe & CSI_PE_MASK;out->pe_changed|=previous^current;out->pe_high|=current;out->pe_low|=CSI_PE_MASK&~current;previous=current;}
    out->pe_last=previous;
    out->csi_if_cfg=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x04U);
    out->csi_mode=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x08U);
    out->csi_hsize=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x44U);
    out->csi_destination=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x50U);
    out->csi_irq_enable=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x70U);
    out->csi_status=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x74U);
    out->csi_geo0=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x80U);
    out->csi_geo1=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x84U);
    out->csi_geo2=*(volatile const uint32_t *)(uintptr_t)(CSI_BASE+0x88U);
    __asm__ volatile("dmb ish":::"memory"); out->captured=1U;
    if (index == 0U)
        csi_buffer_capture_before();
    if (index == CSI_WINDOW_PHASES - 1U) {
        csi_buffer_capture_after();
        csi_window.completed = 1U;
    }
}

#define SMEC_INIT_MAGIC             0x43454D53U /* "SMEC" */
#define SMEC_INIT_VERSION           1U
#define SMEC_INIT_JOB_COUNT         7U
#define SMEC_INIT_SOURCE_BASE       0x40544E68U
#define SMEC_INIT_CACHE_VALUE_BASE  0x42E63028U
#define SMEC_INIT_CACHE_FLAG_BASE   0x42E63424U
#define SMEC_INIT_ANY_FLAG          0x42E63523U
#define SMEC_INIT_CHIP_STATUS       0x42E63524U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t captured;
    uint32_t job_count;
    uint32_t source[SMEC_INIT_JOB_COUNT];
    uint32_t cached[SMEC_INIT_JOB_COUNT];
    uint32_t initialized[SMEC_INIT_JOB_COUNT];
    uint32_t any_initialized;
    uint32_t chip_status_60;
} dposdbg_smec_init_snapshot_t;

__attribute__((section(".data"), used, aligned(4)))
static dposdbg_smec_init_snapshot_t smec_init_snapshot = {
    SMEC_INIT_MAGIC, SMEC_INIT_VERSION, 0U, SMEC_INIT_JOB_COUNT,
    {0U}, {0U}, {0U}, 0U, 0U
};

static const uint16_t smec_source_offsets[SMEC_INIT_JOB_COUNT] = {
    0x01B8U, 0x01C0U, 0x0268U, 0x0270U, 0x026CU, 0x0008U, 0x062CU
};

static const uint8_t smec_source_sizes[SMEC_INIT_JOB_COUNT] = {
    1U, 4U, 4U, 4U, 4U, 2U, 4U
};

static uint32_t read_source_value(uint32_t address, uint32_t size)
{
    if (size == 1U)
        return *(volatile const uint8_t *)(uintptr_t)address;
    if (size == 2U)
        return *(volatile const uint16_t *)(uintptr_t)address;
    return *(volatile const uint32_t *)(uintptr_t)address;
}

void dposdbg_capture_smec_init(void)
{
    uint32_t i;

    for (i = 0U; i < SMEC_INIT_JOB_COUNT; ++i) {
        smec_init_snapshot.source[i] = read_source_value(
            SMEC_INIT_SOURCE_BASE + smec_source_offsets[i],
            smec_source_sizes[i]);
        smec_init_snapshot.cached[i] = *(volatile const uint32_t *)(uintptr_t)
            (SMEC_INIT_CACHE_VALUE_BASE + 4U * (i + 1U));
        smec_init_snapshot.initialized[i] = *(volatile const uint8_t *)(uintptr_t)
            (SMEC_INIT_CACHE_FLAG_BASE + i + 1U);
    }

    smec_init_snapshot.any_initialized =
        *(volatile const uint8_t *)(uintptr_t)SMEC_INIT_ANY_FLAG;
    smec_init_snapshot.chip_status_60 =
        *(volatile const uint8_t *)(uintptr_t)SMEC_INIT_CHIP_STATUS;
    __asm__ volatile ("dmb ish" ::: "memory");
    smec_init_snapshot.captured = 1U;
}

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
static const char smec_init_path[] = "/SMECINIT.BIN";
static const char init_check_path[] = "/INITCHK.BIN";
static const char csi_window_path[] = "/CSIWIN.BIN";
static const char csi_buffer_path[] = "/CSIBUF.BIN";

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

static int smec_init_export(void)
{
    stock_f_open_t stock_f_open =
        (stock_f_open_t)(uintptr_t)STOCK_F_OPEN;
    stock_f_write_t stock_f_write =
        (stock_f_write_t)(uintptr_t)STOCK_F_WRITE;
    stock_f_close_t stock_f_close =
        (stock_f_close_t)(uintptr_t)STOCK_F_CLOSE;
    int result;
    int close_result;

    result = stock_f_open(trace_file_object, smec_init_path,
                          STOCK_FA_WRITE_CREATE);
    if (result != 0)
        return result;
    result = stock_f_write(trace_file_object, &smec_init_snapshot,
                           (uint32_t)sizeof(smec_init_snapshot), 0);
    close_result = stock_f_close(trace_file_object);
    return result != 0 ? result : close_result;
}

static int init_check_export(void)
{
    stock_f_open_t stock_f_open = (stock_f_open_t)(uintptr_t)STOCK_F_OPEN;
    stock_f_write_t stock_f_write = (stock_f_write_t)(uintptr_t)STOCK_F_WRITE;
    stock_f_close_t stock_f_close = (stock_f_close_t)(uintptr_t)STOCK_F_CLOSE;
    int result, close_result;
    result = stock_f_open(trace_file_object, init_check_path, STOCK_FA_WRITE_CREATE);
    if (result != 0) return result;
    result = stock_f_write(trace_file_object, &init_check_snapshot,
                           (uint32_t)sizeof(init_check_snapshot), 0);
    close_result = stock_f_close(trace_file_object);
    return result != 0 ? result : close_result;
}

static int csi_window_export(void)
{
    stock_f_open_t fo=(stock_f_open_t)(uintptr_t)STOCK_F_OPEN;
    stock_f_write_t fw=(stock_f_write_t)(uintptr_t)STOCK_F_WRITE;
    stock_f_close_t fc=(stock_f_close_t)(uintptr_t)STOCK_F_CLOSE;
    int r,cr;
    r=fo(trace_file_object,csi_window_path,STOCK_FA_WRITE_CREATE); if(r!=0)return r;
    r=fw(trace_file_object,&csi_window,(uint32_t)sizeof(csi_window),0);
    cr=fc(trace_file_object); return r!=0?r:cr;
}

static int csi_buffer_export(void)
{
    stock_f_open_t fo = (stock_f_open_t)(uintptr_t)STOCK_F_OPEN;
    stock_f_write_t fw = (stock_f_write_t)(uintptr_t)STOCK_F_WRITE;
    stock_f_close_t fc = (stock_f_close_t)(uintptr_t)STOCK_F_CLOSE;
    int result;
    int close_result;

    result = fo(trace_file_object, csi_buffer_path, STOCK_FA_WRITE_CREATE);
    if (result != 0)
        return result;
    result = fw(trace_file_object, &csi_buffer,
                (uint32_t)sizeof(csi_buffer), 0);
    close_result = fc(trace_file_object);
    return result != 0 ? result : close_result;
}

void dposdbg_after_screenshot(void)
{
    /* Preserve a partial in-flight transaction in the exported snapshot. */
    if (job_active != 0U)
        commit_current_job(JOB_FLAG_TRUNCATED);

    dposdbg_trace.export_count += 1U;
    (void)trace_export();
    (void)smec_init_export();
    (void)init_check_export();
    (void)csi_window_export();
    (void)csi_buffer_export();
}
