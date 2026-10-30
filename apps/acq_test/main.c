#include <stdint.h>

#include "display.h"
#include "acq.h"
#include "fpga_init.h"
#include "fpga_protocol.h"
#include "smec.h"
#include "profile.h"
#include "frontend.h"
#include "console.h"
#include "timer.h"
#include "cache.h"
#include "mmu.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U
#define GREEN 0x07E0U
#define RED   0xF800U

#define CAPTURE_SIZE              0x1800U
#define CAPTURE_POSITION          0U
#define CAPTURE_PERIOD_FIXED      CAPTURE_SIZE
#define CAPTURE_OFFSET_SIMPLE     1U
#define CAPTURE_ORIGIN_FIXED      3U
#define CAPTURE_BANK              0U
#define CAPTURE_TIMEOUT_ITERATIONS 0x01000000U
#define ACQ_READY_MAX_POLLS        0x00100000U
#define ACQ_MODE_DEFAULT           0x01U
#define CAPTURE_FILL              0xA5U

static uint8_t capture_buffer_a[CAPTURE_SIZE]
    __attribute__((aligned(64)));
static uint8_t capture_buffer_b[CAPTURE_SIZE]
    __attribute__((aligned(64)));


static void buffer_fill(volatile uint8_t *data, uint32_t size, uint8_t value)
{
    uint32_t i;

    for (i = 0U; i < size; ++i)
        data[i] = value;

    __asm__ volatile ("dmb sy" ::: "memory");
}

static uint32_t buffer_count_changed(
    const volatile uint8_t *data,
    uint32_t size,
    uint8_t original,
    uint32_t *first,
    uint32_t *last)
{
    uint32_t changed = 0U;
    uint32_t i;

    *first = 0xFFFFFFFFU;
    *last = 0xFFFFFFFFU;

    for (i = 0U; i < size; ++i) {
        if (data[i] != original) {
            if (changed == 0U)
                *first = i;
            *last = i;
            ++changed;
        }
    }

    return changed;
}

static uint32_t buffer_count_different(
    const volatile uint8_t *a,
    const volatile uint8_t *b,
    uint32_t size)
{
    uint32_t different = 0U;
    uint32_t i;
    for (i = 0U; i < size; ++i) {
        if (a[i] != b[i])
            ++different;
    }
    return different;
}
static void put_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[11];
    uint32_t i;

    text[0] = '0';
    text[1] = 'x';
    for (i = 0U; i < 8U; ++i)
        text[2U + i] = hex[(value >> (28U - i * 4U)) & 0x0FU];
    text[10] = '\0';
    console_puts(text);
}

struct byte_stats {
    uint32_t min;
    uint32_t max;
    uint32_t average;
    uint32_t unique;
};
static void buffer_stats(const volatile uint8_t *data, uint32_t size,
    uint32_t start, uint32_t stride, struct byte_stats *stats)
{
    volatile uint32_t seen[8];
    uint32_t sum = 0U;
    uint32_t count = 0U;
    uint32_t i;

    for (i = 0U; i < 8U; ++i)
        seen[i] = 0U;

    stats->min = 0xFFU;
    stats->max = 0U;
    stats->average = 0U;
    stats->unique = 0U;
    for (i = start; i < size; i += stride) {
        uint32_t value = data[i];
        uint32_t word = value >> 5;
        uint32_t bit = 1U << (value & 31U);
        if (value < stats->min) stats->min = value;
        if (value > stats->max) stats->max = value;
        sum += value;
        ++count;
        if ((seen[word] & bit) == 0U) {
            seen[word] |= bit;
            ++stats->unique;
        }
    }
    if (count != 0U) stats->average = sum / count;
}
static int apply_sc3_s36_fixed(
    struct fpga_protocol_transaction_result *last)
{
    static const struct {
        uint8_t command;
        uint32_t value;
    } jobs[] = {
        { 0x0EU, 0x00000020U },
        { 0x0FU, 0x000000FBU },
        { 0x10U, 0x000000FBU },
        { 0x11U, 0x000008F6U },
        { 0x12U, 0x00000FFFU },
        { 0x13U, 0x000067D0U },
        { 0xB8U, 0x00000000U },
    };
    uint32_t i;

    for (i = 0U; i < sizeof(jobs) / sizeof(jobs[0]); ++i) {
        if (!fpga_protocol_parameter_transaction(
                jobs[i].command, jobs[i].value, last))
            return 0;
    }
    __asm__ volatile ("dmb sy" ::: "memory");
    return 1;
}

static int apply_post_s36_fixed(
    struct fpga_protocol_transaction_result *last,
    uint32_t *failed_command)
{
    static const struct { uint8_t command; uint32_t value; } jobs[] = {
        {0x14U,0x00000000U}, {0x15U,0x454B047CU},
        {0x16U,0x3ED1D1EDU}, {0x17U,0x000004AFU},
        {0x18U,0xBF809962U}, {0xB7U,0x00000000U},
    };
    uint32_t i;
    *failed_command = 0U;
    for (i=0U; i<sizeof(jobs)/sizeof(jobs[0]); ++i) {
        if (!fpga_protocol_parameter_transaction(jobs[i].command, jobs[i].value, last)) {
            *failed_command = jobs[i].command; return 0;
        }
    }
    __asm__ volatile ("dmb sy" ::: "memory");
    if (!last->success || last->echo68 != 0xB7U ||
        last->status67 != 0xB7U || last->result66 != 0xB7B7B7B7U) {
        *failed_command = 0xB7U; return 0;
    }
    return 1;
}

static void apply_runtime_vertical_direct(void)
{
    fpga_protocol_write_u8(0x62U, 0x00U);
    fpga_protocol_write_u16_be(0x2AU, 0x0000U);
    fpga_protocol_write_u16_be(0x2DU, 0x0000U);
    fpga_protocol_write_u8(0x2FU, 0x00U);
    fpga_protocol_write_u8(0x29U, 0x00U);
    fpga_protocol_write_u8(0x30U, 0x00U);
    fpga_protocol_write_u8(0x2CU, 0x00U);
    __asm__ volatile ("dmb sy" ::: "memory");
}

static void put_result(const char *label, int pass)
{
    console_puts(label);
    console_set_color(pass ? GREEN : RED);
    console_putln(pass ? "YES" : "NO");
    console_set_color(WHITE);
}

static const char *profile_step_name(uint32_t step)
{
    switch (step) {
    case PROFILE_STEP_TIMEBASE:      return "TIMEBASE TABLE";
    case PROFILE_STEP_MODE:          return "PROFILE MODE";
    case PROFILE_STEP_TIMING:        return "TIMING PROFILE";
    case PROFILE_STEP_LAYOUT:        return "ACQ LAYOUT";
    case PROFILE_STEP_FPGA_APPLY:    return "FPGA PROFILE APPLY";
    case PROFILE_STEP_CHANNEL_ORDER: return "CHANNEL ORDER";
    case PROFILE_STEP_CHANNEL_CAL:   return "CHANNEL CAL";
    case PROFILE_PROGRESS_PARAM_21:  return "FPGA PARAM 21";
    case PROFILE_PROGRESS_COMMIT_C1: return "FPGA COMMIT C1";
    case PROFILE_PROGRESS_SELECT_04: return "FPGA SELECT 04";
    case PROFILE_PROGRESS_PARAM_22:  return "FPGA PARAM 22";
    case PROFILE_PROGRESS_COMMIT_C2: return "FPGA COMMIT C2";
    case PROFILE_PROGRESS_SELECT_05: return "FPGA SELECT 05";
    case PROFILE_PROGRESS_PARAM_23:  return "FPGA PARAM 23";
    case PROFILE_PROGRESS_COMMIT_C3: return "FPGA COMMIT C3";
    case PROFILE_PROGRESS_SELECT_06: return "FPGA SELECT 06";
    case PROFILE_PROGRESS_SELECT_02: return "FPGA SELECT 02";
    case PROFILE_PROGRESS_SELECT_0D: return "FPGA SELECT 0D";
    case PROFILE_PROGRESS_PULSE_07:  return "FPGA APPLY PULSE";
    case PROFILE_PROGRESS_AVS_DELAY: return "FPGA APPLY DELAY";
    default:                         return "COMPLETE";
    }
}

static void profile_progress_screen(
    uint32_t completed_steps,
    uint32_t current_step)
{
    console_clear();
    console_putln("R9C2 PROFILE CHECKPOINT");
    console_putln("------------------------");
    put_result("FPGA INIT     ", 1);
    put_result("SMEC PASS     ", 1);
    console_putln("");
    console_puts("STEPS DONE    "); put_hex32(completed_steps); console_putln("");
    console_puts("CURRENT STEP  "); console_putln(profile_step_name(current_step));
    console_puts("CURRENT BIT   "); put_hex32(current_step); console_putln("");
    console_putln("");
    console_putln(current_step ? "RUNNING" : "PROFILE COMPLETE");
    console_present();
}

int main(void)
{
    struct fpga_init_result hw;
    struct smec_init_result smec;
    struct profile_init_result profile;
    struct frontend_init_result frontend;
    struct acq_prepare_result prepare;
    struct acq_capture_result capture_a;
    struct acq_capture_result capture_b;
    struct fpga_protocol_transaction_result s36_last;
    struct fpga_protocol_transaction_result post_s36_last;
    struct byte_stats stats_a;
    struct byte_stats stats_b;
    uint32_t changed_a;
    uint32_t changed_b;
    uint32_t different_ab;
    uint32_t position_valid;
    uint32_t position_actual;
    uint32_t position_used;
    uint32_t post_s36_failed_command;
    uint32_t first_change;
    uint32_t last_change;
    int fpga_ok;
    int smec_ok;
    int profile_ok;
    int frontend_ok;
    int capture_called;
    int prepare_ok;
    int s36_ok;
    int post_s36_ok;
    int capture_a_ok;
    int capture_b_ok;

    /* Stage 2 hands off with MMU/caches disabled. Own the app runtime. */
    mmu_enable_identity_dram_cache(0);

    display_init();
    console_init(WHITE, BLACK, 2U);
    timer_init();
    console_clear();

    console_putln("STANDALONE ACQUISITION TEST");
    console_putln("---------------------------");
    console_putln("R9C2 STANDALONE PROFILE");
    console_putln("");
    console_present();

    smec.completed_jobs = 0U;
    smec.error = 0;
    profile.completed_steps = 0U;
    profile.failed_step = 0U;
    profile.error = 0;
    frontend.completed_steps = 0U;
    frontend.failed_step = 0U;
    frontend.last_job_result = 0U;
    frontend.last_job_attempts = 0U;
    frontend.last_job_error = 0;
    frontend.success = 0;
    frontend_ok = 0;

    fpga_ok = fpga_init(&hw);
    smec_ok = fpga_ok ? smec_init(&smec) : 0;

    /* Publish the already proven stages before profile_init can block. */
    console_clear();
    console_putln("R9C2 PROFILE CHECKPOINT");
    console_putln("------------------------");
    put_result("FPGA INIT     ", fpga_ok);
    put_result("SMEC PASS     ", smec_ok);
    console_puts("JOBS DONE     "); put_hex32(smec.completed_jobs); console_putln("");
    console_putln("");
    console_putln(smec_ok ? "PROFILE START" : "PROFILE SKIPPED");
    console_present();

    profile_ok = smec_ok ? profile_init(&profile, profile_progress_screen) : 0;
    
    frontend_ok = profile_ok
        ? frontend_init_stock_default(&frontend)
        : 0;
    
    capture_called =
        fpga_ok &&
        smec_ok &&
        profile_ok &&
        frontend_ok;
    
    prepare_ok = 0;
    s36_ok = 0;
    post_s36_ok = 0;
    capture_a_ok = 0;
    capture_b_ok = 0;

    changed_a = 0U;
    changed_b = 0U;
    different_ab = 0U;
    position_valid = 0U;
    position_actual = 0U;
    position_used = CAPTURE_POSITION;
    post_s36_failed_command = 0U;
    prepare.status_before = 0U;
    prepare.status_after = 0U;
    prepare.polls = 0U;
    prepare.ready = 0;
    first_change = 0xFFFFFFFFU;
    last_change = 0xFFFFFFFFU;

    if (capture_called) {
        apply_runtime_vertical_direct();
        s36_ok = apply_sc3_s36_fixed(&s36_last);

        post_s36_ok = s36_ok && apply_post_s36_fixed(
            &post_s36_last, &post_s36_failed_command);
        acq_configure_stream();
        prepare_ok = post_s36_ok && acq_prepare(
            ACQ_MODE_DEFAULT,
            ACQ_READY_MAX_POLLS,
            &prepare);

        if (prepare_ok) {
            position_valid = fpga_protocol_read_u8(0x18U) & 1U;
            if (position_valid != 0U) {
                position_actual = fpga_protocol_read_u16_be(0x14U);
                position_used = position_actual;
            } else {
                /*
                 * Stock simple-mode path:
                 *   offset_63c = 1
                 *   position = (period + offset - origin) % period
                 * Fixed profile: period 0x1800, origin_6f2 3.
                 */
                position_used =
                    (CAPTURE_PERIOD_FIXED + CAPTURE_OFFSET_SIMPLE -
                     CAPTURE_ORIGIN_FIXED) % CAPTURE_PERIOD_FIXED;
            }
        }
        buffer_fill(capture_buffer_a, CAPTURE_SIZE, CAPTURE_FILL);
        cache_clean_range((uintptr_t)capture_buffer_a, CAPTURE_SIZE);
        cache_dsb();
        capture_a_ok = prepare_ok && acq_capture(
            (uintptr_t)capture_buffer_a,
            (uint16_t)position_used,
            CAPTURE_BANK,
            CAPTURE_TIMEOUT_ITERATIONS,
            &capture_a);
        cache_invalidate_range((uintptr_t)capture_buffer_a, CAPTURE_SIZE);
        cache_dsb();
        changed_a = buffer_count_changed(
            capture_buffer_a, CAPTURE_SIZE, CAPTURE_FILL,
            &first_change, &last_change);
        buffer_stats(capture_buffer_a, CAPTURE_SIZE, 0U, 1U, &stats_a);

        buffer_fill(capture_buffer_b, CAPTURE_SIZE, CAPTURE_FILL);
        cache_clean_range((uintptr_t)capture_buffer_b, CAPTURE_SIZE);
        cache_dsb();
        capture_b_ok = capture_a_ok && acq_capture(
            (uintptr_t)capture_buffer_b,
            (uint16_t)position_used,
            1U,
            CAPTURE_TIMEOUT_ITERATIONS,
            &capture_b);
        cache_invalidate_range((uintptr_t)capture_buffer_b, CAPTURE_SIZE);
        cache_dsb();
        changed_b = buffer_count_changed(
            capture_buffer_b, CAPTURE_SIZE, CAPTURE_FILL,
            &first_change, &last_change);
        buffer_stats(capture_buffer_b, CAPTURE_SIZE, 0U, 1U, &stats_b);
        different_ab = buffer_count_different(
            capture_buffer_a, capture_buffer_b, CAPTURE_SIZE);
    }
    /* Compact A/B page: determine whether later frames contain live data. */
    console_clear();
    console_putln("ACQUISITION BANK 0/1");
    console_putln("--------------------------");
    put_result("INIT          ", fpga_ok && smec_ok && profile_ok && frontend_ok);
    put_result("S3.6 FIXED    ", s36_ok);
    put_result("POST S36 B7   ", post_s36_ok);
    put_result("PREPARE ONCE  ", prepare_ok);
    put_result("BANK 0        ", capture_a_ok);
    put_result("BANK 1        ", capture_b_ok);
    console_putln("");
    console_puts("READY PRE/POST"); put_hex32(prepare.status_before); console_puts("/"); put_hex32(prepare.status_after); console_putln("");
    console_puts("READY POLL/RDY"); put_hex32(prepare.polls); console_puts("/"); put_hex32((uint32_t)prepare.ready); console_putln("");

    console_puts("A ST PRE/POST "); put_hex32(capture_a.status_before); console_puts("/"); put_hex32(capture_a.status_after); console_putln("");

    console_puts("B ST PRE/POST "); put_hex32(capture_b.status_before); console_puts("/"); put_hex32(capture_b.status_after); console_putln("");
    console_puts("B MODE/BUF0   "); put_hex32(capture_b.mode); console_puts("/"); put_hex32(capture_b.buffer0); console_putln("");
    console_puts("B IRQ/POLLS   "); put_hex32(capture_b.irq_enable); console_puts("/"); put_hex32(capture_b.wait_polls); console_putln("");
    console_puts("B PE PRE/HIGH "); put_hex32(capture_b.pe_data_before); console_puts("/"); put_hex32(capture_b.pe_data_high); console_putln("");
    console_puts("B PE LOW      "); put_hex32(capture_b.pe_data_low); console_putln("");
    console_puts("B TMR PRE/LOW "); put_hex32(capture_b.timer_before); console_puts("/"); put_hex32(capture_b.timer_low); console_putln("");
    console_puts("B TMR HIGH    "); put_hex32(capture_b.timer_high); console_putln("");
    console_puts("B CFG CSI/GPIO"); put_hex32(capture_b.pe_cfg0_csi); console_puts("/"); put_hex32(capture_b.pe_cfg0_gpio); console_putln("");

    console_puts("A/B BYTES     "); put_hex32(changed_a); console_puts("/"); put_hex32(changed_b); console_putln("");
    console_puts("A/B DIFFERENT "); put_hex32(different_ab); console_putln("");
    console_present();

    for (;;)
        __asm__ volatile ("wfi");
}
