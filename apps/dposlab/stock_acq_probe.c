#include <stdint.h>
#include "stock_acq_probe.h"
#include "../../arch/r11/cache.h"
#include "../../ui/console.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U
#define STOCK_APP_STATE_BASE       0x40544E68U
#define STOCK_RAW_BASE             0x42DF76D4U
#define STOCK_RAW_BANK_STRIDE      0x00002134U
#define STOCK_RAW_ACTIVE_SIZE      0x00001800U
#define STOCK_SELECT_BLOCK         ((void (*)(uint32_t))(uintptr_t)0x4006338CU)
#define STOCK_ACQ_PREPARE          ((void (*)(void))(uintptr_t)0x4003FA10U)
#define EXPERIMENT_C_STOCK_ARM_SEQUENCE 1
#define STOCK_ACQ_SELECTOR09_APPLY ((void (*)(void))(uintptr_t)0x4003F910U)

#define DPOSLAB_EXPERIMENT_C_PLUS_PHASE_SELECT 1
#define DPOSLAB_SYS_INFO_BASE       0x40544E68U
#define DPOSLAB_CAPTURE_PHASE_OFF   0x584U
#define STOCK_CAPTURE_PHASE_SELECT \
    ((void (*)(uint32_t))(uintptr_t)0x4006338CU)
#define STOCK_ACQ_ARM_PULSE       ((void (*)(void))(uintptr_t)0x4003F8E0U)
#define STOCK_ACQ_WAIT_READY       ((void (*)(void))(uintptr_t)0x4003FCC0U)
#define STOCK_ACQ_UPDATE_POSITION  ((void (*)(void))(uintptr_t)0x4003FAE4U)
#define STOCK_ACQ_RUNTIME_MATH     ((void (*)(void))(uintptr_t)0x4003FB90U)
#define STOCK_ACQ_WINDOW_BOUNDS    ((void (*)(void))(uintptr_t)0x40040030U)
#define STOCK_STREAM_CONFIG        ((void (*)(void))(uintptr_t)0x400405E0U)
#define STOCK_CAPTURE_BANKS_0_3    ((void (*)(void))(uintptr_t)0x400406FCU)
#define STOCK_CAPTURE_POSITION_PREP ((void (*)(void))(uintptr_t)0x4003FAB8U)
#define STOCK_CAPTURE_SELECT_BANK   ((void (*)(uint32_t))(uintptr_t)0x4003FA94U)
#define STOCK_FPGA_BUS_MODE         ((void (*)(uint32_t))(uintptr_t)0x40049094U)
#define STOCK_CSI_SET_DESTINATION   ((void (*)(uintptr_t))(uintptr_t)0x40066030U)
#define STOCK_TIMER0_DISABLE        ((void (*)(uint32_t))(uintptr_t)0x40042C40U)
#define STOCK_TIMER0_ENABLE         ((void (*)(uint32_t))(uintptr_t)0x40042C1CU)
#define STOCK_CSI_START             ((void (*)(void))(uintptr_t)0x400660ACU)
#define STOCK_CAPTURE_GATE_HIGH     ((void (*)(void))(uintptr_t)0x4003F9F8U)
#define STOCK_CAPTURE_GATE_LOW      ((void (*)(void))(uintptr_t)0x4003FA04U)
#define STOCK_CSI_FINISH            ((void (*)(void))(uintptr_t)0x40066108U)
#define STOCK_CSI_COMPLETION_SERVICE ((void (*)(void))(uintptr_t)0x40065EB0U)
#define STOCK_IRQ_REGISTER ((void (*)(uint32_t, void (*)(void), uint32_t, uint32_t))(uintptr_t)0x40066BD4U)
#define STOCK_IRQ_RESTORE  ((void (*)(void))(uintptr_t)0x40066B88U)
#define STOCK_IRQ74_SLOT   (*(volatile uint32_t *)(uintptr_t)0x42E67B24U)
#define BOUNDED_COMPLETION_LIMIT    24000000U
#define BOUNDED_EXIT_TIMEOUT        0U
#define BOUNDED_EXIT_HW_DONE        1U
#define BOUNDED_EXIT_SW_DONE        2U
#define STATE8(o)  (*(volatile uint8_t *)(uintptr_t)(STOCK_APP_STATE_BASE + (o)))
#define STATE16(o) (*(volatile uint16_t *)(uintptr_t)(STOCK_APP_STATE_BASE + (o)))
#define STATE32(o) (*(volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + (o)))
#define MMIO32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define CSI_BASE 0x01CB4000U
#define SNAP_MAGIC 0x50414E53U
#define RESULT_MAGIC 0x50414E31U

static const uint8_t sentinels[DPOSLAB_STOCK_BANK_COUNT] = {0xA5U, 0x5AU, 0xC3U, 0x3CU};

volatile struct dposlab_stock_acq_result dposlab_stock_acq_result
    __attribute__((section(".data")));

static void put_hex(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[11];
    uint32_t i;
    text[0] = '0'; text[1] = 'x';
    for (i = 0U; i < 8U; ++i)
        text[2U + i] = hex[(value >> (28U - i * 4U)) & 15U];
    text[10] = 0;
    console_puts(text);
}

volatile uint32_t dposlab_stock_acq_checkpoint
    __attribute__((section(".data"))) = 0U;

static void checkpoint(uint32_t number, const char *text)
{
    dposlab_stock_acq_checkpoint = number;
    __asm__ volatile ("dmb sy" ::: "memory");
    console_init(WHITE, BLACK, 2U);
    console_clear();
    console_putln("DPOSLAB CHECKPOINT BISECT");
    console_putln("--------------------------");
    console_puts("CHECKPOINT ");
    put_hex(number);
    console_putln("");
    console_putln(text);
    console_putln("");
    console_putln("If frozen, report this line.");
    console_present();
}

static uint32_t load_u32(volatile const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t fnv1a(volatile const uint8_t *p, uint32_t count)
{
    uint32_t h = 2166136261U;
    uint32_t i;
    for (i = 0U; i < count; ++i)
        h = (h ^ p[i]) * 16777619U;
    return h;
}

static void snapshot(struct dposlab_stock_snapshot *s)
{
    s->marker = SNAP_MAGIC;
    s->app_030 = STATE32(0x030U);
    s->app_034 = STATE32(0x034U);
    s->app_038 = STATE8(0x038U);
    s->app_290 = STATE32(0x290U);
    s->app_520 = STATE32(0x520U);
    s->app_63c = STATE32(0x63CU);
    s->app_6f2 = STATE16(0x6F2U);
    s->csi_mode = MMIO32(CSI_BASE + 0x08U);
    s->csi_buffer0 = MMIO32(CSI_BASE + 0x50U);
    s->csi_irq_enable = MMIO32(CSI_BASE + 0x70U);
    s->csi_status = MMIO32(CSI_BASE + 0x74U);
    s->pe_cfg0 = MMIO32(0x01C20890U);
    s->pe_data = MMIO32(0x01C208A0U);
    __asm__ volatile ("dmb sy" ::: "memory");
}

static volatile uint8_t *bank_ptr(uint32_t bank)
{
    return (volatile uint8_t *)(uintptr_t)
        (STOCK_RAW_BASE + bank * STOCK_RAW_BANK_STRIDE);
}

static void fill_bank(uint32_t bank)
{
    volatile uint8_t *p = bank_ptr(bank);
    uint32_t i;
    for (i = 0U; i < STOCK_RAW_ACTIVE_SIZE; ++i)
        p[i] = sentinels[bank];
}

static void inspect_bank(uint32_t bank, struct dposlab_stock_bank_stats *out)
{
    volatile const uint8_t *p = bank_ptr(bank);
    uint32_t seen[8];
    uint32_t i;
    for (i = 0U; i < 8U; ++i) seen[i] = 0U;
    out->sentinel = sentinels[bank];
    out->hash_after = fnv1a(p, STOCK_RAW_ACTIVE_SIZE);
    out->changed = 0U;
    out->first_change = 0xFFFFFFFFU;
    out->last_change = 0xFFFFFFFFU;
    out->nonzero = 0U;
    out->minimum = 0xFFU;
    out->maximum = 0U;
    out->unique = 0U;
    for (i = 0U; i < STOCK_RAW_ACTIVE_SIZE; ++i) {
        uint32_t value = p[i];
        uint32_t word = value >> 5;
        uint32_t bit = 1U << (value & 31U);
        if (value != sentinels[bank]) {
            if (out->changed == 0U) out->first_change = i;
            out->last_change = i;
            ++out->changed;
        }
        if (value != 0U) ++out->nonzero;
        if (value < out->minimum) out->minimum = value;
        if (value > out->maximum) out->maximum = value;
        if ((seen[word] & bit) == 0U) { seen[word] |= bit; ++out->unique; }
    }
    out->data0 = load_u32(p + 0U);
    out->data1 = load_u32(p + 4U);
}



/* -------------------------------------------------------------------------
 * Experiment D0, DPOSLAB_MODE=1
 *
 * Passive native-ring test of the complete stock acquisition entry.
 * No sentinels and no manual FPGA/READY/CSI helper calls are used here.
 * The stock function at 0x400413C8 owns the complete acquisition refresh.
 * ------------------------------------------------------------------------- */
#define DPOSLAB_EXPERIMENT_D0_MODE1 1
#define D0_STOCK_FULL_ACQUISITION \
    ((void (*)(void))(uintptr_t)0x400413C8U)
#define D0_SYS_INFO_BASE       0x40544E68U
#define D0_CSI_BUFA            0x01CB4050U
#define D0_RING_BASE           0x42DF76D4U
#define D0_SLOT_STRIDE         0x2134U
#define D0_SLOT_LEN            0x1800U
#define D0_SLOT_COUNT          8U
#define D0_RESULT_MAGIC        0x44305230U /* "D0R0" */
#define DPOSLAB_D0_RUNTIME_DIAGNOSTICS 1
#define DPOSLAB_EXPERIMENT_D01_CACHE 1
#define DPOSLAB_EXPERIMENT_D02_FOUR_ACQ 1
#define DPOSLAB_EXPERIMENT_D03_PHASE_SYNC 1
#define DPOSLAB_EXPERIMENT_D04A_RUNTIME_STEP 1
#define DPOSLAB_EXPERIMENT_D04B_COMPLETE_ITERATION 1
#define DPOSLAB_EXPERIMENT_D04C_FIVE_RUNTIME_ACQ 1
#define D04C_RUNTIME_COUNT 5U
#define D04B_STOCK_COMMIT_STATE \
    ((void (*)(void))(uintptr_t)0x4003BEB4U)
#define D04B_STOCK_CHANNEL_PROCESS \
    ((void (*)(void))(uintptr_t)0x4003C9F4U)
#define D04B_STOCK_ADVANCE_PROGRESS \
    ((void (*)(void))(uintptr_t)0x4003BF98U)
#define D04B_STOCK_WRITE_SELECTOR09 \
    ((void (*)(void))(uintptr_t)0x4003F910U)
#define D04B_STOCK_PULSE_SELECTOR08 \
    ((void (*)(void))(uintptr_t)0x4003F8E0U)
#define D04B_STOCK_RUNTIME_SERVICE \
    ((void (*)(void))(uintptr_t)0x40049B08U)
#define D04A_STOCK_RUNTIME_REARM \
    ((void (*)(void))(uintptr_t)0x4003C520U)
#define D04A_STOCK_RUNTIME_CAPTURE \
    ((void (*)(void))(uintptr_t)0x400417E4U)
#define D04A_READY_TIMEOUT_MS 2000U
#define D04A_READY_MAX_POLLS  200000U
#define D04A_SYSINFO_BASE     0x40544E68U
#define D03_STOCK_FPGA_RUNTIME_SYNC \
    ((void (*)(void))(uintptr_t)0x4003F894U)
#define D03_ROUND_COUNT 4U
#define D02_ACQ_COUNT          4U
#define D02_DELAY_MS           1000U
#define D02_TOUCH_TICK         0x42E68CD8U
#define D01_DCACHE_LINE_SIZE 32U
#define D01_HASH_NOT_READ    0xFFFFFFFFU
#define D0_FPGA_CMD  ((void (*)(uint32_t))(uintptr_t)0x4004927CU)
#define D0_FPGA_READ ((uint32_t (*)(void))(uintptr_t)0x400491B8U)

struct d0_slot_diag {
    uint32_t hash_before;
    uint32_t hash_after;
    uint32_t nonzero_after;
    uint32_t data0_after;
    uint32_t data1_after;
};

struct d0_result {
    uint32_t magic;
    uint32_t returned;
    uint32_t csi_bufa_before;
    uint32_t csi_bufa_after;
    int32_t dma_slot_before;
    int32_t dma_slot_after;
    uint32_t mode_27a;
    uint32_t count_550;
    uint32_t phase_584;
    uint32_t ready_038;
    uint32_t ch1_03e;
    uint32_t ch2_102;
    uint32_t progress_700_before;
    uint32_t progress_700_after;
    uint32_t target_008_before;
    uint32_t target_008_after;
    uint32_t status_6ee_before;
    uint32_t status_6ee_after;
    uint32_t fpga_0a_before;
    uint32_t fpga_0a_after;
    uint32_t fpga_0b_before;
    uint32_t fpga_0b_after;
    struct d0_slot_diag slot[D0_SLOT_COUNT];
};

static volatile struct d0_result g_d0;

static void d01_invalidate_dcache_range(uint32_t address, uint32_t length)
{
    uint32_t line;
    uint32_t end;

    line = address & ~(D01_DCACHE_LINE_SIZE - 1U);
    end = (address + length + D01_DCACHE_LINE_SIZE - 1U) &
          ~(D01_DCACHE_LINE_SIZE - 1U);

    __asm__ volatile ("dsb sy" ::: "memory");
    while (line < end) {
        __asm__ volatile (
            "mcr p15, 0, %0, c7, c6, 1"
            :
            : "r" (line)
            : "memory");
        line += D01_DCACHE_LINE_SIZE;
    }
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
}

static void d01_invalidate_native_ring(void)
{
    uint32_t i;

    for (i = 0U; i < D0_SLOT_COUNT; ++i) {
        d01_invalidate_dcache_range(
            D0_RING_BASE + i * D0_SLOT_STRIDE,
            D0_SLOT_LEN);
    }
}

static uint32_t d0_fpga_read_selector(uint32_t selector)
{
    D0_FPGA_CMD(selector);
    return D0_FPGA_READ() & 0xFFU;
}

static uint32_t d0_hash(const volatile uint8_t *p, uint32_t n)
{
    uint32_t h = 0x811C9DC5U;
    uint32_t i;
    for (i = 0U; i < n; ++i) {
        h ^= (uint32_t)p[i];
        h *= 0x01000193U;
    }
    return h;
}

static uint32_t d0_u32(const volatile uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t d0_dma_slot(uint32_t bufa)
{
    uint32_t address = bufa << 2;
    uint32_t delta;

    if (address < D0_RING_BASE)
        return -1;
    delta = address - D0_RING_BASE;
    if ((delta % D0_SLOT_STRIDE) != 0U)
        return -1;
    delta /= D0_SLOT_STRIDE;
    if (delta >= D0_SLOT_COUNT)
        return -1;
    return (int32_t)delta;
}

static void d0_collect_after(void)
{
    uint32_t i;
    for (i = 0U; i < D0_SLOT_COUNT; ++i) {
        const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)
            (D0_RING_BASE + i * D0_SLOT_STRIDE);
        uint32_t j;
        uint32_t nonzero = 0U;

        g_d0.slot[i].hash_after = d0_hash(p, D0_SLOT_LEN);
        for (j = 0U; j < D0_SLOT_LEN; ++j)
            if (p[j] != 0U)
                ++nonzero;
        g_d0.slot[i].nonzero_after = nonzero;
        g_d0.slot[i].data0_after = d0_u32(p);
        g_d0.slot[i].data1_after = d0_u32(p + 4U);
    }
}

struct d02_acq_diag {
    uint32_t tick_start;
    uint32_t tick_end;
    uint32_t csi_bufa_before;
    uint32_t csi_bufa_after;
    int32_t dma_slot_before;
    int32_t dma_slot_after;
    uint32_t fpga_0a_before;
    uint32_t fpga_0a_after;
    uint32_t fpga_0b_before;
    uint32_t fpga_0b_after;
    uint32_t phase_before_sync;
    uint32_t phase_after_sync;
    uint32_t phase_after_acq;
    uint32_t total_nonzero;
    uint32_t min_nonzero;
    uint32_t max_nonzero;
    uint32_t hash_mix;
};

struct d02_result {
    uint32_t magic;
    uint32_t completed;
    uint32_t initial_tick;
    uint32_t final_tick;
    struct d02_acq_diag acq[D02_ACQ_COUNT];
};

static volatile struct d02_result g_d02;

static uint32_t d02_tick(void)
{
    return *(volatile uint32_t *)(uintptr_t)D02_TOUCH_TICK;
}

static void __attribute__((unused)) d02_wait_ms(uint32_t delay_ms)
{
    uint32_t start = d02_tick();

    while ((uint32_t)(d02_tick() - start) < delay_ms)
        __asm__ volatile ("nop");

    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
}

static void d02_measure_ring(volatile struct d02_acq_diag *r)
{
    uint32_t i;
    uint32_t total = 0U;
    uint32_t minimum = D0_SLOT_LEN;
    uint32_t maximum = 0U;
    uint32_t mix = 0x811C9DC5U;

    d01_invalidate_native_ring();

    for (i = 0U; i < D0_SLOT_COUNT; ++i) {
        const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)
            (D0_RING_BASE + i * D0_SLOT_STRIDE);
        uint32_t j;
        uint32_t nonzero = 0U;
        uint32_t hash;

        hash = d0_hash(p, D0_SLOT_LEN);
        for (j = 0U; j < D0_SLOT_LEN; ++j) {
            if (p[j] != 0U)
                ++nonzero;
        }

        total += nonzero;
        if (nonzero < minimum)
            minimum = nonzero;
        if (nonzero > maximum)
            maximum = nonzero;

        mix ^= hash;
        mix *= 0x01000193U;
        mix ^= nonzero;
        mix *= 0x01000193U;
    }

    r->total_nonzero = total;
    r->min_nonzero = minimum;
    r->max_nonzero = maximum;
    r->hash_mix = mix;
}

struct d04a_snapshot {
    uint32_t phase_0a;
    uint32_t status_0b;
    uint32_t csi_bufa;
    int32_t dma_slot;
    uint32_t sys_038;
    uint32_t sys_6ee;
    uint32_t sys_6f0;
    uint32_t sys_6fc;
    uint32_t sys_700;
};

struct d04a_result {
    uint32_t magic;
    uint32_t stage;
    uint32_t ready;
    uint32_t poll_count;
    uint32_t poll_elapsed_ms;
    struct d04a_snapshot boot;
    struct d04a_snapshot full_after;
    struct d04a_snapshot rearm_after;
    struct d04a_snapshot runtime_after;
    struct d02_acq_diag full_ring;
    struct d02_acq_diag runtime_ring;
};

static volatile struct d04a_result g_d04a;

static uint32_t d04a_load8(uint32_t offset)
{
    return *(volatile uint8_t *)(uintptr_t)(D04A_SYSINFO_BASE + offset);
}

static uint32_t d04a_load16(uint32_t offset)
{
    return *(volatile uint16_t *)(uintptr_t)(D04A_SYSINFO_BASE + offset);
}

static uint32_t d04a_load32(uint32_t offset)
{
    return *(volatile uint32_t *)(uintptr_t)(D04A_SYSINFO_BASE + offset);
}

static void d04a_snapshot(volatile struct d04a_snapshot *r)
{
    r->phase_0a = d0_fpga_read_selector(0x0AU);
    r->status_0b = d0_fpga_read_selector(0x0BU);
    r->csi_bufa = *(volatile uint32_t *)(uintptr_t)D0_CSI_BUFA;
    r->dma_slot = d0_dma_slot(r->csi_bufa);
    r->sys_038 = d04a_load8(0x038U);
    r->sys_6ee = d04a_load8(0x6EEU);
    r->sys_6f0 = d04a_load16(0x6F0U);
    r->sys_6fc = d04a_load32(0x6FCU);
    r->sys_700 = d04a_load32(0x700U);
}

static uint32_t d04a_poll_ready(uint32_t timeout_ms,
                                uint32_t max_polls,
                                uint32_t *poll_count,
                                uint32_t *elapsed_ms)
{
    uint32_t start = d02_tick();
    uint32_t count = 0U;
    uint32_t status = 0U;

    while (count < max_polls) {
        status = d0_fpga_read_selector(0x0BU);
        ++count;

        if ((status & 1U) != 0U)
            break;

        if ((uint32_t)(d02_tick() - start) >= timeout_ms)
            break;
    }

    *poll_count = count;
    *elapsed_ms = (uint32_t)(d02_tick() - start);
    return status & 1U;
}

struct d04b_result {
    uint32_t magic;
    uint32_t stage;
    uint32_t ready;
    uint32_t poll_count;
    uint32_t poll_elapsed_ms;
    struct d04a_snapshot boot;
    struct d04a_snapshot full_after;
    struct d04a_snapshot rearm_after;
    struct d04a_snapshot capture_after;
    struct d04a_snapshot commit_after;
    struct d04a_snapshot process_after;
    struct d04a_snapshot advance_after;
    struct d04a_snapshot rearm2_after;
    struct d02_acq_diag full_ring;
    struct d02_acq_diag capture_ring;
    struct d02_acq_diag final_ring;
};

static volatile struct d04b_result g_d04b;

static void d04b_barrier(void)
{
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
}

static void __attribute__((unused)) d04b_run(void)
{
    g_d04b.magic = 0x44344231U; /* "D4B1" */
    g_d04b.stage = 0U;
    g_d04b.ready = 0U;
    g_d04b.poll_count = 0U;
    g_d04b.poll_elapsed_ms = 0U;

    d04a_snapshot(&g_d04b.boot);

    checkpoint(0x70U, "D04B FULL ACQ: ENTER");
    d04b_barrier();
    D0_STOCK_FULL_ACQUISITION();
    d04b_barrier();
    checkpoint(0x71U, "D04B FULL ACQ: RETURN");
    d04a_snapshot(&g_d04b.full_after);
    d02_measure_ring(&g_d04b.full_ring);
    g_d04b.stage = 1U;

    checkpoint(0x72U, "D04B REARM: ENTER");
    d04b_barrier();
    D04A_STOCK_RUNTIME_REARM();
    d04b_barrier();
    checkpoint(0x73U, "D04B REARM: RETURN");
    d04a_snapshot(&g_d04b.rearm_after);
    g_d04b.stage = 2U;

    g_d04b.ready = d04a_poll_ready(
        D04A_READY_TIMEOUT_MS,
        D04A_READY_MAX_POLLS,
        (uint32_t *)&g_d04b.poll_count,
        (uint32_t *)&g_d04b.poll_elapsed_ms);
    g_d04b.stage = 3U;

    if (g_d04b.ready == 0U)
        return;

    checkpoint(0x74U, "D04B RUNTIME CAP: ENTER");
    d04b_barrier();
    D04A_STOCK_RUNTIME_CAPTURE();
    d04b_barrier();
    checkpoint(0x75U, "D04B RUNTIME CAP: RETURN");
    d04a_snapshot(&g_d04b.capture_after);
    d02_measure_ring(&g_d04b.capture_ring);
    g_d04b.stage = 4U;

    checkpoint(0x76U, "D04B COMMIT: ENTER");
    D04B_STOCK_COMMIT_STATE();
    d04b_barrier();
    checkpoint(0x77U, "D04B COMMIT: RETURN");
    d04a_snapshot(&g_d04b.commit_after);
    g_d04b.stage = 5U;

    checkpoint(0x78U, "D04B PROCESS: ENTER");
    D04B_STOCK_CHANNEL_PROCESS();
    d04b_barrier();
    checkpoint(0x79U, "D04B PROCESS: RETURN");
    d04a_snapshot(&g_d04b.process_after);
    g_d04b.stage = 6U;

    checkpoint(0x7AU, "D04B ADVANCE: ENTER");
    D04B_STOCK_ADVANCE_PROGRESS();
    d04b_barrier();
    checkpoint(0x7BU, "D04B ADVANCE: RETURN");
    d04a_snapshot(&g_d04b.advance_after);
    g_d04b.stage = 7U;

    checkpoint(0x7CU, "D04B WRITE09: ENTER");
    D04B_STOCK_WRITE_SELECTOR09();
    d04b_barrier();
    checkpoint(0x7DU, "D04B WRITE09: RETURN");

    checkpoint(0x7EU, "D04B PULSE08: ENTER");
    D04B_STOCK_PULSE_SELECTOR08();
    d04b_barrier();
    checkpoint(0x7FU, "D04B PULSE08: RETURN");

    D04B_STOCK_RUNTIME_SERVICE();
    d04b_barrier();
    d04a_snapshot(&g_d04b.rearm2_after);
    d02_measure_ring(&g_d04b.final_ring);
    g_d04b.stage = 8U;
}

static void d04b_put_short(const char *name,
                           const volatile struct d04a_snapshot *r)
{
    console_puts(name);
    console_puts(" PH/B ");
    put_hex(r->phase_0a);
    console_puts("/");
    put_hex(r->status_0b);
    console_puts(" D ");
    put_hex((uint32_t)r->dma_slot);
    console_putln("");

    console_puts(" 38/E/F0/FC/700 ");
    put_hex(r->sys_038);
    console_puts("/");
    put_hex(r->sys_6ee);
    console_puts("/");
    put_hex(r->sys_6f0);
    console_puts("/");
    put_hex(r->sys_6fc);
    console_puts("/");
    put_hex(r->sys_700);
    console_putln("");
}

static void d04a_put_ring(const volatile struct d02_acq_diag *r);

static void __attribute__((unused)) d04b_monitor(void)
{
    console_clear();
    console_putln("EXP D0.4B FULL RUNTIME ITER");
    console_puts("STAGE/MAGIC ");
    put_hex(g_d04b.stage);
    console_puts("/");
    put_hex(g_d04b.magic);
    console_putln("");

    d04b_put_short("FULL", &g_d04b.full_after);
    d04a_put_ring(&g_d04b.full_ring);

    d04b_put_short("REARM", &g_d04b.rearm_after);
    console_puts("READY/POLLS/MS ");
    put_hex(g_d04b.ready);
    console_puts("/");
    put_hex(g_d04b.poll_count);
    console_puts("/");
    put_hex(g_d04b.poll_elapsed_ms);
    console_putln("");

    if (g_d04b.stage >= 4U) {
        d04b_put_short("CAP", &g_d04b.capture_after);
        d04a_put_ring(&g_d04b.capture_ring);
    }
    if (g_d04b.stage >= 5U)
        d04b_put_short("COMMIT", &g_d04b.commit_after);
    if (g_d04b.stage >= 6U)
        d04b_put_short("PROCESS", &g_d04b.process_after);
    if (g_d04b.stage >= 7U)
        d04b_put_short("ADVANCE", &g_d04b.advance_after);
    if (g_d04b.stage >= 8U) {
        d04b_put_short("REARM2", &g_d04b.rearm2_after);
        d04a_put_ring(&g_d04b.final_ring);
    }

    if (g_d04b.stage < 8U)
        console_putln("STOPPED BEFORE FINAL STAGE");
    console_putln("POWER CYCLE TO EXIT");
    console_present();
}

struct d04c_cycle {
    uint32_t ready;
    uint32_t poll_count;
    uint32_t poll_elapsed_ms;
    struct d04a_snapshot before_capture;
    struct d04a_snapshot after_capture;
    struct d04a_snapshot after_advance;
    struct d04a_snapshot after_rearm;
    struct d02_acq_diag ring;
};

struct d04c_result {
    uint32_t magic;
    uint32_t stage;
    uint32_t completed;
    struct d04a_snapshot full_after;
    struct d04a_snapshot initial_rearm;
    struct d02_acq_diag full_ring;
    struct d04c_cycle cycle[D04C_RUNTIME_COUNT];
};

static volatile struct d04c_result g_d04c;

static void d04c_run(void)
{
    uint32_t i;

    g_d04c.magic = 0x44344335U; /* "D4C5" */
    g_d04c.stage = 0U;
    g_d04c.completed = 0U;

    checkpoint(0x80U, "D04C FULL ACQ: ENTER");
    d04b_barrier();
    D0_STOCK_FULL_ACQUISITION();
    d04b_barrier();
    checkpoint(0x81U, "D04C FULL ACQ: RETURN");
    d04a_snapshot(&g_d04c.full_after);
    d02_measure_ring(&g_d04c.full_ring);
    g_d04c.stage = 1U;

    checkpoint(0x82U, "D04C INITIAL REARM: ENTER");
    d04b_barrier();
    D04A_STOCK_RUNTIME_REARM();
    d04b_barrier();
    checkpoint(0x83U, "D04C INITIAL REARM: RETURN");
    d04a_snapshot(&g_d04c.initial_rearm);
    g_d04c.stage = 2U;

    for (i = 0U; i < D04C_RUNTIME_COUNT; ++i) {
        volatile struct d04c_cycle *c = &g_d04c.cycle[i];

        c->ready = d04a_poll_ready(
            D04A_READY_TIMEOUT_MS,
            D04A_READY_MAX_POLLS,
            (uint32_t *)&c->poll_count,
            (uint32_t *)&c->poll_elapsed_ms);
        d04a_snapshot(&c->before_capture);
        g_d04c.stage = 0x10U + i * 4U;

        if (c->ready == 0U)
            return;

        checkpoint(0x90U + i, "D04C RUNTIME CAP: ENTER");
        d04b_barrier();
        D04A_STOCK_RUNTIME_CAPTURE();
        d04b_barrier();
        checkpoint(0x98U + i, "D04C RUNTIME CAP: RETURN");
        d04a_snapshot(&c->after_capture);
        d02_measure_ring(&c->ring);
        g_d04c.stage = 0x11U + i * 4U;

        D04B_STOCK_COMMIT_STATE();
        d04b_barrier();
        D04B_STOCK_CHANNEL_PROCESS();
        d04b_barrier();
        g_d04c.stage = 0x12U + i * 4U;

        D04B_STOCK_ADVANCE_PROGRESS();
        d04b_barrier();
        d04a_snapshot(&c->after_advance);

        D04B_STOCK_WRITE_SELECTOR09();
        d04b_barrier();
        D04B_STOCK_PULSE_SELECTOR08();
        d04b_barrier();
        D04B_STOCK_RUNTIME_SERVICE();
        d04b_barrier();
        d04a_snapshot(&c->after_rearm);

        g_d04c.completed = i + 1U;
        g_d04c.stage = 0x13U + i * 4U;
    }

    g_d04c.stage = 0xFFU;
}

static void d04c_put_ring(const volatile struct d02_acq_diag *r)
{
    console_puts(" NZ/HASH ");
    put_hex(r->total_nonzero);
    console_puts("/");
    put_hex(r->hash_mix);
    console_putln("");
}

static void d04c_put_cycle(uint32_t index,
                           const volatile struct d04c_cycle *c)
{
    console_puts("C");
    console_puts(index == 0U ? "1" : index == 1U ? "2" :
                 index == 2U ? "3" : index == 3U ? "4" : "5");
    console_puts(" RD/P/MS ");
    put_hex(c->ready);
    console_puts("/");
    put_hex(c->poll_count);
    console_puts("/");
    put_hex(c->poll_elapsed_ms);
    console_putln("");

    console_puts(" B PH/0B/F0/FC/700 ");
    put_hex(c->before_capture.phase_0a);
    console_puts("/");
    put_hex(c->before_capture.status_0b);
    console_puts("/");
    put_hex(c->before_capture.sys_6f0);
    console_puts("/");
    put_hex(c->before_capture.sys_6fc);
    console_puts("/");
    put_hex(c->before_capture.sys_700);
    console_putln("");

    console_puts(" A F0/FC/700 ");
    put_hex(c->after_advance.sys_6f0);
    console_puts("/");
    put_hex(c->after_advance.sys_6fc);
    console_puts("/");
    put_hex(c->after_advance.sys_700);
    console_putln("");

    console_puts(" R 0B/F0/FC/700 ");
    put_hex(c->after_rearm.status_0b);
    console_puts("/");
    put_hex(c->after_rearm.sys_6f0);
    console_puts("/");
    put_hex(c->after_rearm.sys_6fc);
    console_puts("/");
    put_hex(c->after_rearm.sys_700);
    console_putln("");

    d04c_put_ring(&c->ring);
}

static void d04c_monitor(void)
{
    uint32_t i;

    console_clear();
    console_putln("EXP D0.4C FIVE RUNTIME ACQ");
    console_puts("STAGE/DONE/MAGIC ");
    put_hex(g_d04c.stage);
    console_puts("/");
    put_hex(g_d04c.completed);
    console_puts("/");
    put_hex(g_d04c.magic);
    console_putln("");

    console_puts("FULL PH/0B/F0/FC/700 ");
    put_hex(g_d04c.full_after.phase_0a);
    console_puts("/");
    put_hex(g_d04c.full_after.status_0b);
    console_puts("/");
    put_hex(g_d04c.full_after.sys_6f0);
    console_puts("/");
    put_hex(g_d04c.full_after.sys_6fc);
    console_puts("/");
    put_hex(g_d04c.full_after.sys_700);
    console_putln("");
    d04c_put_ring(&g_d04c.full_ring);

    console_puts("INIT R 0B/F0/FC/700 ");
    put_hex(g_d04c.initial_rearm.status_0b);
    console_puts("/");
    put_hex(g_d04c.initial_rearm.sys_6f0);
    console_puts("/");
    put_hex(g_d04c.initial_rearm.sys_6fc);
    console_puts("/");
    put_hex(g_d04c.initial_rearm.sys_700);
    console_putln("");

    for (i = 0U; i < g_d04c.completed; ++i)
        d04c_put_cycle(i, &g_d04c.cycle[i]);

    if (g_d04c.completed < D04C_RUNTIME_COUNT) {
        console_puts("STOP CYCLE ");
        put_hex(g_d04c.completed + 1U);
        console_putln("");
    }

    console_putln("POWER CYCLE TO EXIT");
    console_present();
}

static void __attribute__((unused)) d04a_run(void)
{
    g_d04a.magic = 0x44344131U; /* "D4A1" */
    g_d04a.stage = 0U;
    g_d04a.ready = 0U;
    g_d04a.poll_count = 0U;
    g_d04a.poll_elapsed_ms = 0U;

    d04a_snapshot(&g_d04a.boot);

    checkpoint(0x60U, "D04A FULL ACQ: ENTER");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    D0_STOCK_FULL_ACQUISITION();
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    checkpoint(0x61U, "D04A FULL ACQ: RETURN");

    d04a_snapshot(&g_d04a.full_after);
    d02_measure_ring(&g_d04a.full_ring);
    g_d04a.stage = 1U;

    checkpoint(0x62U, "D04A REARM: ENTER");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    D04A_STOCK_RUNTIME_REARM();
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    checkpoint(0x63U, "D04A REARM: RETURN");

    d04a_snapshot(&g_d04a.rearm_after);
    g_d04a.stage = 2U;

    g_d04a.ready = d04a_poll_ready(
        D04A_READY_TIMEOUT_MS,
        D04A_READY_MAX_POLLS,
        (uint32_t *)&g_d04a.poll_count,
        (uint32_t *)&g_d04a.poll_elapsed_ms);
    g_d04a.stage = 3U;

    if (g_d04a.ready == 0U)
        return;

    checkpoint(0x64U, "D04A RUNTIME CAP: ENTER");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    D04A_STOCK_RUNTIME_CAPTURE();
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    checkpoint(0x65U, "D04A RUNTIME CAP: RETURN");

    d04a_snapshot(&g_d04a.runtime_after);
    d02_measure_ring(&g_d04a.runtime_ring);
    g_d04a.stage = 4U;
}

static void d04a_put_snapshot(const volatile struct d04a_snapshot *r)
{
    console_puts(" PH/0B ");
    put_hex(r->phase_0a);
    console_puts("/");
    put_hex(r->status_0b);
    console_puts(" DMA ");
    put_hex((uint32_t)r->dma_slot);
    console_putln("");

    console_puts(" 38/6EE/6F0 ");
    put_hex(r->sys_038);
    console_puts("/");
    put_hex(r->sys_6ee);
    console_puts("/");
    put_hex(r->sys_6f0);
    console_putln("");

    console_puts(" 6FC/700 ");
    put_hex(r->sys_6fc);
    console_puts("/");
    put_hex(r->sys_700);
    console_putln("");
}

static void d04a_put_ring(const volatile struct d02_acq_diag *r)
{
    console_puts(" NZ T/MIN/MAX ");
    put_hex(r->total_nonzero);
    console_puts("/");
    put_hex(r->min_nonzero);
    console_puts("/");
    put_hex(r->max_nonzero);
    console_putln("");

    console_puts(" HASH MIX ");
    put_hex(r->hash_mix);
    console_putln("");
}

static void __attribute__((unused)) d04a_monitor(void)
{
    console_clear();
    console_putln("EXP D0.4A RUNTIME STEP");
    console_puts("STAGE/MAGIC ");
    put_hex(g_d04a.stage);
    console_puts("/");
    put_hex(g_d04a.magic);
    console_putln("");

    console_putln("BOOT");
    d04a_put_snapshot(&g_d04a.boot);

    console_putln("FULL AFTER");
    d04a_put_snapshot(&g_d04a.full_after);
    d04a_put_ring(&g_d04a.full_ring);

    console_putln("REARM AFTER");
    d04a_put_snapshot(&g_d04a.rearm_after);
    console_puts("READY/POLLS/MS ");
    put_hex(g_d04a.ready);
    console_puts("/");
    put_hex(g_d04a.poll_count);
    console_puts("/");
    put_hex(g_d04a.poll_elapsed_ms);
    console_putln("");

    if (g_d04a.stage >= 4U) {
        console_putln("RUNTIME AFTER");
        d04a_put_snapshot(&g_d04a.runtime_after);
        d04a_put_ring(&g_d04a.runtime_ring);
    } else {
        console_putln("RUNTIME CAPTURE SKIPPED");
    }

    console_putln("POWER CYCLE TO EXIT");
    console_present();
}

static void __attribute__((unused)) d02_run(void)
{
    uint32_t i;

    g_d02.magic = 0x44303334U; /* "D034" */
    g_d02.completed = 0U;
    g_d02.initial_tick = d02_tick();
    g_d02.final_tick = g_d02.initial_tick;

    for (i = 0U; i < D03_ROUND_COUNT; ++i) {
        volatile struct d02_acq_diag *r = &g_d02.acq[i];

        r->tick_start = d02_tick();
        r->csi_bufa_before = *(volatile uint32_t *)(uintptr_t)D0_CSI_BUFA;
        r->dma_slot_before = d0_dma_slot(r->csi_bufa_before);
        r->phase_before_sync = d0_fpga_read_selector(0x0AU);
        r->fpga_0b_before = d0_fpga_read_selector(0x0BU);

        if (i != 0U) {
            checkpoint(0x40U + i, "D03 FPGA SYNC: ENTER");
            __asm__ volatile ("dsb sy" ::: "memory");
            __asm__ volatile ("isb sy" ::: "memory");
            D03_STOCK_FPGA_RUNTIME_SYNC();
            __asm__ volatile ("dsb sy" ::: "memory");
            __asm__ volatile ("isb sy" ::: "memory");
            checkpoint(0x44U + i, "D03 FPGA SYNC: RETURN");
        }

        r->phase_after_sync = d0_fpga_read_selector(0x0AU);
        r->fpga_0a_before = r->phase_after_sync;

        checkpoint(0x50U + i, "D03 STOCK ACQ: ENTER");
        __asm__ volatile ("dsb sy" ::: "memory");
        __asm__ volatile ("isb sy" ::: "memory");
        D0_STOCK_FULL_ACQUISITION();
        __asm__ volatile ("dsb sy" ::: "memory");
        __asm__ volatile ("isb sy" ::: "memory");

        r->csi_bufa_after = *(volatile uint32_t *)(uintptr_t)D0_CSI_BUFA;
        r->dma_slot_after = d0_dma_slot(r->csi_bufa_after);
        r->phase_after_acq = d0_fpga_read_selector(0x0AU);
        r->fpga_0a_after = r->phase_after_acq;
        r->fpga_0b_after = d0_fpga_read_selector(0x0BU);
        r->tick_end = d02_tick();

        d02_measure_ring(r);
        g_d02.completed = i + 1U;
        g_d02.final_tick = r->tick_end;
        checkpoint(0x54U + i, "D03 STOCK ACQ: RETURN");
    }
}

static void __attribute__((unused)) d02_monitor(void)
{
    uint32_t i;

    console_clear();
    console_putln("EXP D0.3 STOCK FPGA SYNC");
    console_putln("ACQ1 BASE; SYNC BEFORE 2-4");
    console_puts("DONE/MAGIC ");
    put_hex(g_d02.completed);
    console_puts("/");
    put_hex(g_d02.magic);
    console_putln("");

    for (i = 0U; i < D03_ROUND_COUNT; ++i) {
        const volatile struct d02_acq_diag *r = &g_d02.acq[i];

        console_puts("ROUND");
        console_puts(i == 0U ? "1" : i == 1U ? "2" : i == 2U ? "3" : "4");
        console_puts(" PH B/S/A ");
        put_hex(r->phase_before_sync);
        console_puts("/");
        put_hex(r->phase_after_sync);
        console_puts("/");
        put_hex(r->phase_after_acq);
        console_putln("");

        console_puts(" DMA B/A ");
        put_hex((uint32_t)r->dma_slot_before);
        console_puts("/");
        put_hex((uint32_t)r->dma_slot_after);
        console_puts(" 0B ");
        put_hex(r->fpga_0b_after);
        console_putln("");

        console_puts(" NZ TOT/MIN/MAX ");
        put_hex(r->total_nonzero);
        console_puts("/");
        put_hex(r->min_nonzero);
        console_puts("/");
        put_hex(r->max_nonzero);
        console_putln("");

        console_puts(" HASH MIX ");
        put_hex(r->hash_mix);
        console_putln("");
    }

    console_putln("POWER CYCLE TO EXIT");
    console_present();
}

static void __attribute__((unused)) d0_run(void)
{
    uint32_t i;
    volatile uint8_t *si = (volatile uint8_t *)(uintptr_t)D0_SYS_INFO_BASE;

    g_d0.magic = D0_RESULT_MAGIC;
    g_d0.returned = 0U;
    g_d0.mode_27a = si[0x27AU];
    g_d0.count_550 = *(volatile uint16_t *)(uintptr_t)(D0_SYS_INFO_BASE + 0x550U);
    g_d0.phase_584 = *(volatile uint16_t *)(uintptr_t)(D0_SYS_INFO_BASE + 0x584U);
    g_d0.ready_038 = si[0x038U];
    g_d0.ch1_03e = si[0x03EU];
    g_d0.ch2_102 = si[0x102U];
    g_d0.csi_bufa_before = *(volatile uint32_t *)(uintptr_t)D0_CSI_BUFA;
    g_d0.dma_slot_before = d0_dma_slot(g_d0.csi_bufa_before);
    g_d0.progress_700_before =
        *(volatile uint32_t *)(uintptr_t)(D0_SYS_INFO_BASE + 0x700U);
    g_d0.target_008_before =
        *(volatile uint16_t *)(uintptr_t)(D0_SYS_INFO_BASE + 0x008U);
    g_d0.status_6ee_before = si[0x6EEU];
    g_d0.fpga_0a_before = d0_fpga_read_selector(0x0AU);
    g_d0.fpga_0b_before = d0_fpga_read_selector(0x0BU);

    for (i = 0U; i < D0_SLOT_COUNT; ++i) {
        g_d0.slot[i].hash_before = 0U;
        g_d0.slot[i].hash_after = 0U;
        g_d0.slot[i].nonzero_after = 0U;
        g_d0.slot[i].data0_after = 0U;
        g_d0.slot[i].data1_after = 0U;
    }
    /* D0.1: do not touch the ring before DMA.  Reading it here would fill
     * D-cache with the previous zero contents and hide DMA writes later. */
    for (i = 0U; i < D0_SLOT_COUNT; ++i)
        g_d0.slot[i].hash_before = D01_HASH_NOT_READ;

    checkpoint(0x21U, "D01 FULL STOCK ACQ: ENTER");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    D0_STOCK_FULL_ACQUISITION();
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    g_d0.returned = 1U;
    checkpoint(0x22U, "D01 FULL STOCK ACQ: RETURN");

    g_d0.csi_bufa_after = *(volatile uint32_t *)(uintptr_t)D0_CSI_BUFA;
    g_d0.dma_slot_after = d0_dma_slot(g_d0.csi_bufa_after);
    g_d0.progress_700_after =
        *(volatile uint32_t *)(uintptr_t)(D0_SYS_INFO_BASE + 0x700U);
    g_d0.target_008_after =
        *(volatile uint16_t *)(uintptr_t)(D0_SYS_INFO_BASE + 0x008U);
    g_d0.status_6ee_after = si[0x6EEU];
    g_d0.fpga_0a_after = d0_fpga_read_selector(0x0AU);
    g_d0.fpga_0b_after = d0_fpga_read_selector(0x0BU);

    /* Discard potentially stale lines without cleaning them back over DMA. */
    d01_invalidate_native_ring();
    d0_collect_after();
}

static void __attribute__((unused)) d0_monitor(void)
{
    uint32_t i;

    console_clear();
    console_putln("EXP D0.1 CACHE-SAFE READ");
    console_putln("NO PRE-READ, DCIMVAC AFTER DMA");
    console_puts("RETURN/MAGIC ");
    put_hex(g_d0.returned);
    console_puts("/");
    put_hex(g_d0.magic);
    console_putln("");

    console_puts("MODE/CNT/PHASE ");
    put_hex(g_d0.mode_27a);
    console_puts("/");
    put_hex(g_d0.count_550);
    console_puts("/");
    put_hex(g_d0.phase_584);
    console_putln("");

    console_puts("READY/CH1/CH2 ");
    put_hex(g_d0.ready_038);
    console_puts("/");
    put_hex(g_d0.ch1_03e);
    console_puts("/");
    put_hex(g_d0.ch2_102);
    console_putln("");

    console_puts("CSI BUFA B/A ");
    put_hex(g_d0.csi_bufa_before);
    console_puts("/");
    put_hex(g_d0.csi_bufa_after);
    console_putln("");
    console_puts("DMA SLOT B/A ");
    put_hex((uint32_t)g_d0.dma_slot_before);
    console_puts("/");
    put_hex((uint32_t)g_d0.dma_slot_after);
    console_putln("");

    console_puts("PROG700 B/A ");
    put_hex(g_d0.progress_700_before);
    console_puts("/");
    put_hex(g_d0.progress_700_after);
    console_putln("");
    console_puts("TARGET08 B/A ");
    put_hex(g_d0.target_008_before);
    console_puts("/");
    put_hex(g_d0.target_008_after);
    console_putln("");
    console_puts("STATE6EE B/A ");
    put_hex(g_d0.status_6ee_before);
    console_puts("/");
    put_hex(g_d0.status_6ee_after);
    console_putln("");
    console_puts("FPGA 0A B/A ");
    put_hex(g_d0.fpga_0a_before);
    console_puts("/");
    put_hex(g_d0.fpga_0a_after);
    console_putln("");
    console_puts("FPGA 0B B/A ");
    put_hex(g_d0.fpga_0b_before);
    console_puts("/");
    put_hex(g_d0.fpga_0b_after);
    console_putln("");

    for (i = 0U; i < D0_SLOT_COUNT; ++i) {
        console_puts("S");
        console_puts(i == 0U ? "0" : i == 1U ? "1" : i == 2U ? "2" :
                     i == 3U ? "3" : i == 4U ? "4" : i == 5U ? "5" :
                     i == 6U ? "6" : "7");
        console_puts(" HASH B/A ");
        put_hex(g_d0.slot[i].hash_before);
        console_puts("/");
        put_hex(g_d0.slot[i].hash_after);
        console_putln("");
        console_puts("   NZ DATA0 ");
        put_hex(g_d0.slot[i].nonzero_after);
        console_puts("/");
        put_hex(g_d0.slot[i].data0_after);
        console_putln("");
    }

    console_putln("POWER CYCLE TO EXIT");
    console_present();
}

void dposlab_stock_acq_probe(void)
{
#if DPOSLAB_MODE == 1
    d04c_run();
    return;
#endif

    uint16_t capture_phase_base;
    uint8_t capture_phase_arg;

    struct dposlab_stock_acq_result *out =
        (struct dposlab_stock_acq_result *)(uintptr_t)&dposlab_stock_acq_result;
    uint32_t bank;
    uint32_t computed_position;

    out->marker = RESULT_MAGIC;
    out->raw_base = STOCK_RAW_BASE;
    out->status_before = 0U; out->status_after = 0U; out->ready = 0U;
    out->period = 0U; out->offset = 0U; out->origin = 0U; out->position = 0U;

    /* Entry is the Win_OpenMainBank call-site: SysConfigExec has just returned. */
    snapshot(&out->s1_after_sysconfig);
    checkpoint(0x01U, "S1 AFTER SYSCONFIG OK");

    for (bank = 0U; bank < DPOSLAB_STOCK_BANK_COUNT; ++bank)
        fill_bank(bank);
    if (cache_dcache_enabled())
        cache_clean_range(STOCK_RAW_BASE,
            DPOSLAB_STOCK_BANK_COUNT * STOCK_RAW_BANK_STRIDE);
    cache_dsb();
    for (bank = 0U; bank < DPOSLAB_STOCK_BANK_COUNT; ++bank)
        out->bank[bank].hash_before = fnv1a(bank_ptr(bank), STOCK_RAW_ACTIVE_SIZE);
    snapshot(&out->s2_sentinel_ready);
    checkpoint(0x02U, "S2 SENTINELS READY OK");

    checkpoint(0x03U, "SELECT BLOCK: ENTER");
    STOCK_SELECT_BLOCK(0U);
    checkpoint(0x04U, "SELECT BLOCK: RETURN");
    snapshot(&out->s3_acq_entry);

    checkpoint(0x05U, "C+ PHASE+ARM: ENTER");
        capture_phase_base = *(volatile uint16_t *)(uintptr_t)
        (DPOSLAB_SYS_INFO_BASE + DPOSLAB_CAPTURE_PHASE_OFF);
    capture_phase_arg = (uint8_t)(capture_phase_base + 0U);
    STOCK_CAPTURE_PHASE_SELECT((uint32_t)capture_phase_arg);
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
STOCK_ACQ_SELECTOR09_APPLY();
    STOCK_ACQ_PREPARE();
    STOCK_ACQ_ARM_PULSE();
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
    checkpoint(0x06U, "C+ PHASE+ARM: RETURN");
    out->status_before = STATE8(0x038U);

    checkpoint(0x07U, "WAIT READY: ENTER");
    STOCK_ACQ_WAIT_READY();
    checkpoint(0x08U, "WAIT READY: RETURN");
    out->status_after = STATE8(0x038U);
    out->ready = out->status_after != 0U ? 1U : 0U;

    if (out->ready != 0U) {
        checkpoint(0x09U, "UPDATE POSITION: ENTER");
        STOCK_ACQ_UPDATE_POSITION();
        checkpoint(0x0AU, "UPDATE POSITION: RETURN");

        checkpoint(0x0BU, "RUNTIME MATH: ENTER");
        STOCK_ACQ_RUNTIME_MATH();
        checkpoint(0x0CU, "RUNTIME MATH: RETURN");

        checkpoint(0x0DU, "WINDOW BOUNDS: ENTER");
        STOCK_ACQ_WINDOW_BOUNDS();
        checkpoint(0x0EU, "WINDOW BOUNDS: RETURN");
        out->period = STATE32(0x520U);
        out->offset = STATE32(0x63CU);
        out->origin = STATE16(0x6F2U);
        if (out->period != 0U) {
            computed_position =
                (out->period + out->offset - out->origin) % out->period;
            out->position = computed_position;
        }
        checkpoint(0x0FU, "STREAM CONFIG: ENTER");
        STOCK_STREAM_CONFIG();
        checkpoint(0x10U, "STREAM CONFIG: RETURN");

        checkpoint(0x11U, "STOCK BANKS 0-3: ENTER");
        __asm__ volatile ("dsb sy" ::: "memory");
        __asm__ volatile ("isb sy" ::: "memory");
        STOCK_CAPTURE_BANKS_0_3();
        __asm__ volatile ("dsb sy" ::: "memory");
        __asm__ volatile ("isb sy" ::: "memory");
        checkpoint(0x12U, "STOCK BANKS 0-3: RETURN");
    }

    snapshot(&out->s5_acq_return);
    checkpoint(0x13U, "S5 SNAPSHOT: OK");
    if (cache_dcache_enabled())
        cache_invalidate_range(STOCK_RAW_BASE,
            DPOSLAB_STOCK_BANK_COUNT * STOCK_RAW_BANK_STRIDE);
    cache_dsb();
    for (bank = 0U; bank < DPOSLAB_STOCK_BANK_COUNT; ++bank)
        inspect_bank(bank, &out->bank[bank]);
    checkpoint(0x14U, "BUFFER INSPECTION: OK");
}

static void print_snapshot(const char *name,
                           const volatile struct dposlab_stock_snapshot *s)
{
    console_putln(name);
    console_puts("  030/034 "); put_hex(s->app_030); console_puts("/"); put_hex(s->app_034); console_putln("");
    console_puts("  038/290 "); put_hex(s->app_038); console_puts("/"); put_hex(s->app_290); console_putln("");
    console_puts("  520/63C "); put_hex(s->app_520); console_puts("/"); put_hex(s->app_63c); console_putln("");
    console_puts("  CSI M/B  "); put_hex(s->csi_mode); console_puts("/"); put_hex(s->csi_buffer0); console_putln("");
    console_puts("  IRQ/STAT "); put_hex(s->csi_irq_enable); console_puts("/"); put_hex(s->csi_status); console_putln("");
}

void dposlab_stock_acq_monitor(void)
{
#if DPOSLAB_MODE == 1
    d04c_monitor();
    return;
#endif

    const volatile struct dposlab_stock_acq_result *r = &dposlab_stock_acq_result;
    uint32_t i;
    console_init(WHITE, BLACK, 2U); console_clear();
    console_putln("DPOSLAB STOCK SNAPSHOT ACQ");
    console_putln("--------------------------");
    console_puts("READY/PRE/POST "); put_hex(r->ready); console_puts("/"); put_hex(r->status_before); console_puts("/"); put_hex(r->status_after); console_putln("");
    console_puts("POS/PERIOD "); put_hex(r->position); console_puts("/"); put_hex(r->period); console_putln("");
    print_snapshot("S1 AFTER SYSCONFIG", &r->s1_after_sysconfig);
    print_snapshot("S2 SENTINELS READY", &r->s2_sentinel_ready);
    print_snapshot("S3 ACQ ENTRY", &r->s3_acq_entry);
    print_snapshot("S5 ACQ RETURN", &r->s5_acq_return);
    console_putln("BOUNDED COMPLETION DIAG");
    console_puts(" EXIT/ITERS "); put_hex(r->bounded.exit_reason); console_puts("/"); put_hex(r->bounded.iterations); console_putln("");
    console_puts(" HW/SW DONE "); put_hex(r->bounded.hw_done); console_puts("/"); put_hex(r->bounded.sw_done); console_putln("");
    console_puts(" STATUS F/L "); put_hex(r->bounded.status_first); console_puts("/"); put_hex(r->bounded.status_last); console_putln("");
    console_puts(" STATUS OR  "); put_hex(r->bounded.status_or); console_putln("");
    console_puts(" MODE B/S/F "); put_hex(r->bounded.mode_before); console_puts("/"); put_hex(r->bounded.mode_after_start); console_puts("/"); put_hex(r->bounded.mode_final); console_putln("");
    console_puts(" BUF EXP/ACT"); put_hex(r->bounded.buffer_expected); console_puts("/"); put_hex(r->bounded.buffer_actual); console_putln("");
    console_puts(" IRQ ENABLE "); put_hex(r->bounded.irq_enable); console_putln("");
    console_puts(" APP70E B/A "); put_hex(r->bounded.app_70e_before); console_puts("/"); put_hex(r->bounded.app_70e_after); console_putln("");
    console_puts(" SERVICE CALL"); put_hex(r->bounded.service_called); console_putln("");
    console_puts(" SVC STAT B/A"); put_hex(r->bounded.status_before_service); console_puts("/"); put_hex(r->bounded.status_after_service); console_putln("");
    console_puts(" SVC 70E B/A "); put_hex(r->bounded.app_70e_before_service); console_puts("/"); put_hex(r->bounded.app_70e_after_service); console_putln("");
    console_puts(" ACK/PUBLISH "); put_hex(r->bounded.service_acked_done); console_puts("/"); put_hex(r->bounded.service_published_done); console_putln("");
    console_puts(" IRQ74 SLOT B"); put_hex(r->bounded.irq74_slot_before); console_putln("");
    console_puts(" IRQ74 SLOT A"); put_hex(r->bounded.irq74_slot_after); console_putln("");
    console_puts(" IRQ74 REG   "); put_hex(r->bounded.irq74_registered); console_putln("");
    for (i = 0U; i < DPOSLAB_STOCK_BANK_COUNT; ++i) {
        console_puts("B"); console_puts(i == 0U ? "0" : i == 1U ? "1" : i == 2U ? "2" : "3");
        console_puts(" SENT/CHG "); put_hex(r->bank[i].sentinel); console_puts("/"); put_hex(r->bank[i].changed); console_putln("");
        console_puts("  FIRST/LAST "); put_hex(r->bank[i].first_change); console_puts("/"); put_hex(r->bank[i].last_change); console_putln("");
        console_puts("  HASH B/A   "); put_hex(r->bank[i].hash_before); console_puts("/"); put_hex(r->bank[i].hash_after); console_putln("");
        console_puts("  DATA0/1    "); put_hex(r->bank[i].data0); console_puts("/"); put_hex(r->bank[i].data1); console_putln("");
    }
    console_present();

    /* Final screen for the real stock 0-3 capture.  The bounded diagnostic
     * structure is intentionally not used here because the stock wrapper now
     * owns completion and returns only after all four banks have completed. */
    console_clear();
    console_putln("EXP C+ FPGA PHASE SELECT");
    console_putln("--------------------------");
    console_putln("STOCK INIT + PHASE + ARM");
    console_putln("RETURNED FROM 0x400406FC");
    console_putln("");

    console_puts("PHASE BASE/ARG ");
    put_hex((uint32_t)*(volatile uint16_t *)(uintptr_t)
        (DPOSLAB_SYS_INFO_BASE + DPOSLAB_CAPTURE_PHASE_OFF));
    console_puts("/");
    put_hex((uint32_t)(uint8_t)*(volatile uint16_t *)(uintptr_t)
        (DPOSLAB_SYS_INFO_BASE + DPOSLAB_CAPTURE_PHASE_OFF));
    console_putln("");

    console_puts("S1/S5 CSI STAT ");
    put_hex(r->s1_after_sysconfig.csi_status);
    console_puts("/");
    put_hex(r->s5_acq_return.csi_status);
    console_putln("");
    console_puts("S1/S5 CSI MODE ");
    put_hex(r->s1_after_sysconfig.csi_mode);
    console_puts("/");
    put_hex(r->s5_acq_return.csi_mode);
    console_putln("");
    console_puts("S1/S5 BUFFER0  ");
    put_hex(r->s1_after_sysconfig.csi_buffer0);
    console_puts("/");
    put_hex(r->s5_acq_return.csi_buffer0);
    console_putln("");
    console_puts("S1/S5 IRQ EN   ");
    put_hex(r->s1_after_sysconfig.csi_irq_enable);
    console_puts("/");
    put_hex(r->s5_acq_return.csi_irq_enable);
    console_putln("");
    console_putln("");

    for (i = 0U; i < DPOSLAB_STOCK_BANK_COUNT; ++i) {
        console_puts("B");
        console_puts(i == 0U ? "0" : i == 1U ? "1" : i == 2U ? "2" : "3");
        console_puts(" SENT/CHG ");
        put_hex(r->bank[i].sentinel);
        console_puts("/");
        put_hex(r->bank[i].changed);
        console_putln("");

        console_puts("   HASH B/A  ");
        put_hex(r->bank[i].hash_before);
        console_puts("/");
        put_hex(r->bank[i].hash_after);
        console_putln("");

        console_puts("   FIRST/LAST");
        put_hex(r->bank[i].first_change);
        console_puts("/");
        put_hex(r->bank[i].last_change);
        console_putln("");

        console_puts("   DATA0/1   ");
        put_hex(r->bank[i].data0);
        console_puts("/");
        put_hex(r->bank[i].data1);
        console_putln("");
    }

    console_putln("");
    console_putln("POWER CYCLE TO EXIT");
    console_present();
}
