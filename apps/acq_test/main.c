#include <stdint.h>

#include "display.h"
#include "acq.h"
#include "fpga_init.h"
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
#define CAPTURE_BANK              0U
#define CAPTURE_TIMEOUT_ITERATIONS 0x01000000U
#define CAPTURE_FILL              0xA5U

static uint8_t capture_buffer[CAPTURE_SIZE]
    __attribute__((aligned(64)));

static uint32_t buffer_hash(const volatile uint8_t *data, uint32_t size)
{
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0U; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }

    return hash;
}

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
    struct acq_capture_result capture;
    uint32_t hash_before;
    uint32_t hash_after;
    uint32_t changed_bytes;
    uint32_t first_change;
    uint32_t last_change;
    int fpga_ok;
    int smec_ok;
    int profile_ok;
    int frontend_ok;
    int capture_called;
    int capture_ok;

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
    
    capture_ok = 0;

    hash_before = 0U;
    hash_after = 0U;
    changed_bytes = 0U;
    first_change = 0xFFFFFFFFU;
    last_change = 0xFFFFFFFFU;

    if (capture_called) {
        buffer_fill(capture_buffer, CAPTURE_SIZE, CAPTURE_FILL);
        hash_before = buffer_hash(capture_buffer, CAPTURE_SIZE);

        /* CPU -> DMA ownership: publish the sentinel and remove dirty lines. */
        cache_clean_range((uintptr_t)capture_buffer, CAPTURE_SIZE);
        cache_dsb();

        capture_ok = acq_capture(
            (uintptr_t)capture_buffer,
            CAPTURE_POSITION,
            CAPTURE_BANK,
            CAPTURE_TIMEOUT_ITERATIONS,
            &capture);

        /* DMA -> CPU ownership: discard any cached pre-capture contents. */
        cache_invalidate_range((uintptr_t)capture_buffer, CAPTURE_SIZE);
        cache_dsb();

        hash_after = buffer_hash(capture_buffer, CAPTURE_SIZE);
        changed_bytes = buffer_count_changed(
            capture_buffer,
            CAPTURE_SIZE,
            CAPTURE_FILL,
            &first_change,
            &last_change);
    }

    /* Fresh compact page: progress output must not consume final diagnostics. */
    console_clear();
    console_putln("R9D STANDALONE CAPTURE");
    console_putln("----------------------");
    put_result("FPGA INIT     ", fpga_ok);
    put_result("SMEC PASS     ", smec_ok);
    put_result("PROFILE PASS  ", profile_ok);
    console_puts("STEPS DONE    "); put_hex32(profile.completed_steps); console_putln("");
    console_puts("FAILED STEP   "); put_hex32(profile.failed_step); console_putln("");
    put_result("FRONTEND PASS ", frontend_ok);
    console_puts("FE STEPS      "); put_hex32(frontend.completed_steps); console_putln("");
    console_puts("FE FAILED     "); put_hex32(frontend.failed_step); console_putln("");
    console_puts("FE JOB RESULT "); put_hex32(frontend.last_job_result); console_putln("");
    console_puts("FE ATTEMPTS   "); put_hex32(frontend.last_job_attempts); console_putln("");
    console_puts("FE JOB ERROR  "); put_hex32((uint32_t)frontend.last_job_error); console_putln("");
    console_putln("");

    put_result("CAPTURE CALL  ", capture_called);
    put_result("CAPTURE PASS  ", capture_ok);

    if (capture_called) {
        put_result("HW DONE       ", capture.done);
        console_puts("CSI BEFORE    "); put_hex32(capture.status_before); console_putln("");
        console_puts("CSI AFTER     "); put_hex32(capture.status_after); console_putln("");
        console_puts("CSI MODE      "); put_hex32(capture.mode); console_putln("");
        console_puts("CSI BUF0      "); put_hex32(capture.buffer0); console_putln("");
        console_puts("CSI IRQ EN    "); put_hex32(capture.irq_enable); console_putln("");
        console_puts("WAIT POLLS    "); put_hex32(capture.wait_polls); console_putln("");
        console_putln("");
        put_result("RAM CHANGED   ", changed_bytes != 0U);
        console_puts("CHANGED BYTES "); put_hex32(changed_bytes); console_putln("");
        console_puts("FIRST CHANGE  "); put_hex32(first_change); console_putln("");
        console_puts("LAST CHANGE   "); put_hex32(last_change); console_putln("");
        console_puts("HASH BEFORE   "); put_hex32(hash_before); console_putln("");
        console_puts("HASH AFTER    "); put_hex32(hash_after); console_putln("");
    }

    console_present();

    for (;;)
        __asm__ volatile ("wfi");
}
