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
#include "sdmmc0.h"
#include "fat.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U
#define GREEN 0x07E0U
#define RED   0xF800U

#define CAPTURE_SIZE              0x1800U
#define CAPTURE_POSITION          3U
#define CAPTURE_PERIOD_FIXED      CAPTURE_SIZE
#define CAPTURE_OFFSET_SIMPLE     1U
#define CAPTURE_ORIGIN_FIXED      3U
#define CAPTURE_BANK              0U
#define CAPTURE_TIMEOUT_ITERATIONS 0x01000000U
#define ACQ_READY_MAX_POLLS        0x00100000U
#define ACQ_MODE_DEFAULT           0x01U
#define CAPTURE_FILL              0xA5U
/* One stock-like prepare followed by raw export of FPGA banks 0..7. */
#define ACQ_TEST_CYCLES              1U
#define ACQ_TEST_BANKS              8U
#define RAW8_TRANSPOSE_ROWS          8U


static uint8_t capture_buffer[ACQ_TEST_BANKS][CAPTURE_SIZE]
    __attribute__((aligned(64)));
static uint32_t cycle_changed[ACQ_TEST_CYCLES][ACQ_TEST_BANKS];
static uint32_t cycle_hash[ACQ_TEST_CYCLES][ACQ_TEST_BANKS];
static uint32_t cycle_nonzero[ACQ_TEST_CYCLES][ACQ_TEST_BANKS];
static uint32_t cycle_first[ACQ_TEST_CYCLES][ACQ_TEST_BANKS];
static uint32_t cycle_last[ACQ_TEST_CYCLES][ACQ_TEST_BANKS];
static uint32_t cycle_diff_b0[ACQ_TEST_CYCLES][ACQ_TEST_BANKS];
static uint32_t cycle_bank_ok[ACQ_TEST_CYCLES];
static uint32_t cycle_ready_first[ACQ_TEST_CYCLES];
static uint32_t cycle_ready_final[ACQ_TEST_CYCLES];
static uint32_t cycle_ready_polls[ACQ_TEST_CYCLES];
static uint32_t cycle_pointer_valid[ACQ_TEST_CYCLES];
static uint32_t cycle_live_pointer[ACQ_TEST_CYCLES];
static uint32_t cycle_runtime_290[ACQ_TEST_CYCLES];
static uint32_t cycle_runtime_63c[ACQ_TEST_CYCLES];
static uint32_t cycle_capture_position[ACQ_TEST_CYCLES];
static uint32_t cycle_apply_kind[ACQ_TEST_CYCLES];
static uint32_t cycle_apply_ret[ACQ_TEST_CYCLES];

static void buffer_fill(volatile uint8_t *data, uint32_t size, uint8_t value)
{
    uint32_t i;

    for (i = 0U; i < size; ++i)
        data[i] = value;

    __asm__ volatile ("dmb sy" ::: "memory");
}

static int apply_factory_acquisition_geometry(
    struct fpga_protocol_transaction_result *last,
    uint32_t *failed_register)
{
    static const struct {
        uint8_t reg;
        uint8_t length;
        uint32_t value;
    } registers[] = {
        { 0x01U, 1U, 0x00000014U },
        { 0x02U, 4U, 0x00000040U },
        { 0x03U, 4U, 0x00000040U },
        { 0x04U, 4U, 0x000003E8U },
        { 0x05U, 4U, 0x00000064U },
        { 0x06U, 2U, 0x000003B6U },
        { 0x07U, 4U, 0x000000C8U },
    };
    uint32_t i;

    *failed_register = 0U;

    for (i = 0U;
         i < sizeof(registers) / sizeof(registers[0]);
         ++i) {
        if (!fpga_protocol_register_write(
                registers[i].reg,
                registers[i].length,
                registers[i].value,
                last)) {
            *failed_register = registers[i].reg;
            return 0;
        }
    }

    if (!fpga_protocol_register_commit(0xB4U, last)) {
        *failed_register = 0xB4U;
        return 0;
    }

    __asm__ volatile ("dmb sy" ::: "memory");
    return 1;
}

static uint32_t buffer_hash_fnv1a(
    const volatile uint8_t *data,
    uint32_t size)
{
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0U; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }

    return hash;
}

static uint32_t buffer_count_nonzero(const volatile uint8_t *data, uint32_t size)
{
    uint32_t count = 0U;
    uint32_t i;
    for (i = 0U; i < size; ++i)
        if (data[i] != 0U) ++count;
    return count;
}

static uint32_t buffer_count_different(const volatile uint8_t *left,
                                       const volatile uint8_t *right,
                                       uint32_t size)
{
    uint32_t count = 0U;
    uint32_t i;
    for (i = 0U; i < size; ++i)
        if (left[i] != right[i]) ++count;
    return count;
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

static void put_hex16(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[5];
    uint32_t i;
    for (i = 0U; i < 4U; ++i)
        text[i] = hex[(value >> (12U - i * 4U)) & 0x0FU];
    text[4] = '\0';
    console_puts(text);
}

static void put_hex8(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[3];
    text[0] = hex[(value >> 4U) & 0x0FU];
    text[1] = hex[value & 0x0FU];
    text[2] = '\0';
    console_puts(text);
}

static void print_raw8_transpose(uint32_t rows)
{
    uint32_t index;
    uint32_t bank;
    if (rows > CAPTURE_SIZE) rows = CAPTURE_SIZE;
    console_putln("IDX B0 B1 B2 B3 B4 B5 B6 B7");
    for (index = 0U; index < rows; ++index) {
        put_hex16(index); console_puts(" ");
        for (bank = 0U; bank < ACQ_TEST_BANKS; ++bank) {
            put_hex8(capture_buffer[bank][index]);
            if (bank + 1U != ACQ_TEST_BANKS) console_puts(" ");
        }
        console_putln("");
    }
}

static int __attribute__((unused)) apply_horizontal_20us(
    struct fpga_protocol_transaction_result *last,
    uint32_t *failed_command)
{
    static const struct { uint8_t command; uint32_t value; } jobs[] = {
        {0x21U,0x0000003CU}, {0xC1U,0x00000000U},
        {0x23U,0x0000003EU}, {0xC3U,0x00000000U},
        {0x1CU,0x000000E1U}, {0xBCU,0x00000000U},
        {0x20U,0x000000C3U}, {0xC0U,0x00000000U},
        {0x12U,0x00000FFFU}, {0x13U,0x000067D0U},
        {0xB8U,0x00000000U},
    };
    uint32_t i;

    *failed_command = 0U;
    for (i = 0U; i < sizeof(jobs) / sizeof(jobs[0]); ++i) {
        if (!fpga_protocol_parameter_transaction(
                jobs[i].command, jobs[i].value, last)) {
            *failed_command = jobs[i].command;
            return 0;
        }
    }

    __asm__ volatile ("dmb sy" ::: "memory");
    return 1;
}

static int apply_trigger_stock_default(
    struct fpga_protocol_transaction_result *last,
    uint32_t *failed_command)
{
    static const struct { uint8_t command; uint32_t value; } jobs[] = {
        {0x0EU,0x00000020U},
        {0x0FU,0x000000FBU},
        {0x10U,0x000000FBU},
        {0x11U,0x000008F6U},
        {0x12U,0x00000FFFU},
        {0x13U,0x000067D0U},
        {0xB8U,0x00000000U},
    };
    uint32_t i;

    *failed_command = 0U;

    /*
     * HW/static-confirmed stock default trigger state:
     *   mode=0, CH1 wire encoding=2, rising edge=0, level=0xFB.
     * Keep selectors 0x61 and 0x0C out of this first experiment because
     * their source-state values are not yet independently HW-confirmed.
     */
    fpga_protocol_write_u8(0x61U, 0x00U); 
    fpga_protocol_write_u16_be(0x0CU, 0x0031U);

    fpga_protocol_write_u8(0x1AU, 0x00U);
    fpga_protocol_write_u8(0x15U, 0x02U);
    fpga_protocol_write_u8(0x16U, 0x00U);

    for (i = 0U; i < sizeof(jobs) / sizeof(jobs[0]); ++i) {
        if (!fpga_protocol_parameter_transaction(
                jobs[i].command, jobs[i].value, last)) {
            *failed_command = jobs[i].command;
            return 0;
        }
    }

    //fpga_protocol_write_u8(0x17U, 0xFBU);
    fpga_protocol_write_u16_be(0x1BU, 0x0BFDU);
    fpga_protocol_write_u16_be(0x1CU, 0x0BFDU);

    __asm__ volatile ("dmb sy" ::: "memory");
    return 1;
}

static int __attribute__((unused)) apply_horizontal_10us(
    struct fpga_protocol_transaction_result *last,
    uint32_t *failed_command)
{
    static const struct { uint8_t command; uint32_t value; } jobs[] = {
        {0x21U,0x0000003EU}, {0xC1U,0x00000000U},
        {0x23U,0x0000003FU}, {0xC3U,0x00000000U},
        {0x1CU,0x000000C3U}, {0xBCU,0x00000000U},
        {0x20U,0x000000E1U}, {0xC0U,0x00000000U},
        {0x12U,0x00001013U}, {0x13U,0x00003994U},
        {0xB8U,0x00000000U},
    };
    uint32_t i;
    *failed_command = 0U;
    for (i = 0U; i < sizeof(jobs) / sizeof(jobs[0]); ++i) {
        if (!fpga_protocol_parameter_transaction(jobs[i].command, jobs[i].value, last)) {
            *failed_command = jobs[i].command;
            return 0;
        }
    }
    __asm__ volatile ("dmb sy" ::: "memory");
    return 1;
}

static void put_result(const char *label, int pass)
{
    console_puts(label);
    console_set_color(pass ? GREEN : RED);
    console_putln(pass ? "YES" : "NO");
    console_set_color(WHITE);
}

static void profile_progress_screen(
    uint32_t completed_steps,
    uint32_t current_step)
{
    (void)completed_steps;
    (void)current_step;
}

int main(void)
{
    struct smec_init_result smec;
    struct profile_init_result profile;
    struct frontend_init_result frontend;
    struct acq_prepare_result prepare;
    struct acq_capture_result capture;
    struct fpga_protocol_transaction_result horizontal_last;
    struct fpga_protocol_transaction_result geometry_last;
    sdmmc0_info_t sd_info;
    fat_volume_t fat_volume;
    fat_file_t acq20_file;
    uint32_t trigger_failed_command = 0U;
    uint32_t first_change;
    uint32_t last_change;
    uint32_t completed_cycles = 0U;
    uint32_t geometry_failed_register = 0U;
    uint32_t cycle;
    uint32_t bank;
    int fpga_ok;
    int smec_ok;
    int profile_ok;
    int frontend_ok;
    int init_ok;
    int sc2_ok = 0;
    int horizontal_ok = 0;
    int trigger_ok = 0;
    int prepare_ok;
    int capture_ok;
    int test_ok = 0;
    int geometry_ok;
    int sd_result = -999;
    int fat_mount_result = -999;
    int acq20_find_result = -999;
    int acq20_write_result = -999;

    mmu_enable_identity_dram_cache(0);
    display_init();
    console_init(WHITE, BLACK, 2U);
    timer_init();

    /* Timing test: no console rendering before acquisition. */

    fpga_ok = fpga_init(0);
    smec_ok = fpga_ok ? smec_init(&smec) : 0;

    /* Timing test: silent between SMEC and profile/SC2. */

    geometry_ok = smec_ok && apply_factory_acquisition_geometry( &geometry_last, &geometry_failed_register);

    profile_ok = geometry_ok ? profile_init(&profile, profile_progress_screen) : 0;
    frontend_ok = profile_ok ? frontend_init_stock_default(&frontend) : 0;
    init_ok = fpga_ok && smec_ok && geometry_ok && profile_ok && frontend_ok;

    for (cycle = 0U; cycle < ACQ_TEST_CYCLES; ++cycle) {
        cycle_bank_ok[cycle] = 0U;
        cycle_ready_first[cycle] = 0U;
        cycle_ready_final[cycle] = 0U;
        cycle_ready_polls[cycle] = 0U;
        cycle_pointer_valid[cycle] = 0U;
        cycle_live_pointer[cycle] = 0U;
        cycle_runtime_290[cycle] = 0U;
        cycle_runtime_63c[cycle] = 0U;
        cycle_capture_position[cycle] = 0U;
        cycle_apply_kind[cycle] = 0U;
        cycle_apply_ret[cycle] = 1U;
        for (bank = 0U; bank < ACQ_TEST_BANKS; ++bank) {
            cycle_changed[cycle][bank] = 0U;
            cycle_hash[cycle][bank] = 0U;
            cycle_nonzero[cycle][bank] = 0U;
            cycle_first[cycle][bank] = 0xFFFFFFFFU;
            cycle_last[cycle][bank] = 0xFFFFFFFFU;
            cycle_diff_b0[cycle][bank] = 0U;
        }
    }

    for (bank = 0U; bank < ACQ_TEST_BANKS; ++bank)
        buffer_fill(capture_buffer[bank], CAPTURE_SIZE, CAPTURE_FILL);

    if (init_ok) {
        /* frontend_init_stock_default() now owns the complete stock SC2 flow. */
        sc2_ok = frontend_ok;
        horizontal_ok = sc2_ok;
        trigger_ok = horizontal_ok && apply_trigger_stock_default(
            &horizontal_last, &trigger_failed_command);
        if (trigger_ok) {
            acq_configure_stream();
            test_ok = 1;
            for (cycle = 0U; cycle < ACQ_TEST_CYCLES; ++cycle) {
                /*
                 * Fixed 20 us/div test. No per-cycle horizontal apply runs;
                 * isolate the effect of the one stock-default trigger apply.
                 */

                prepare_ok = acq_prepare(
                    ACQ_MODE_DEFAULT, ACQ_READY_MAX_POLLS, &prepare);
                cycle_ready_first[cycle] = prepare.status_before;
                cycle_ready_final[cycle] = prepare.status_after;
                cycle_ready_polls[cycle] = prepare.polls;
                cycle_pointer_valid[cycle] = (uint32_t)prepare.pointer_valid;
                cycle_live_pointer[cycle] = prepare.live_pointer;
                cycle_runtime_290[cycle] = prepare.runtime_290;
                cycle_runtime_63c[cycle] = prepare.runtime_63c;
                cycle_capture_position[cycle] = prepare.capture_position;
                if (!prepare_ok) {
                    test_ok = 0;
                    break;
                }
                for (bank = 0U; bank < ACQ_TEST_BANKS; ++bank) {
                    uint8_t *destination = capture_buffer[bank];

                    buffer_fill(destination, CAPTURE_SIZE, CAPTURE_FILL);
                    cache_clean_range((uintptr_t)destination, CAPTURE_SIZE);
                    cache_dsb();
                    capture_ok = acq_capture(
                        (uintptr_t)destination,
                        (uint16_t)CAPTURE_POSITION,
                        (uint8_t)bank,
                        CAPTURE_TIMEOUT_ITERATIONS,
                        &capture);
                    cache_invalidate_range(
                        (uintptr_t)destination, CAPTURE_SIZE);
                    cache_dsb();
                    cycle_changed[cycle][bank] = buffer_count_changed(
                        destination, CAPTURE_SIZE, CAPTURE_FILL,
                        &first_change, &last_change);
                    cycle_first[cycle][bank] = first_change;
                    cycle_last[cycle][bank] = last_change;
                    cycle_nonzero[cycle][bank] =
                        buffer_count_nonzero(destination, CAPTURE_SIZE);
                    cycle_hash[cycle][bank] =
                        buffer_hash_fnv1a(destination, CAPTURE_SIZE);
                    if (!capture_ok) {
                        test_ok = 0;
                        break;
                    }
                    cycle_bank_ok[cycle] |= 1U << bank;
                }
                if (!test_ok)
                    break;

                cycle_diff_b0[cycle][0] = 0U;
                for (bank = 1U; bank < ACQ_TEST_BANKS; ++bank) {
                    cycle_diff_b0[cycle][bank] = buffer_count_different(
                        capture_buffer[0], capture_buffer[bank], CAPTURE_SIZE);
                }

                completed_cycles = cycle + 1U;
            }
        }
    }

    if (test_ok && completed_cycles == ACQ_TEST_CYCLES) {
        static const char acq20_name[11] = {
            'A','C','Q','2','0',' ',' ',' ','B','I','N'
        };

        sd_result = sdmmc0_init(&sd_info);
        if (sd_result == 0)
            fat_mount_result = fat_mount_first(&fat_volume);
        if (fat_mount_result == 0) {
            acq20_find_result = fat_find_root(
                &fat_volume, acq20_name, &acq20_file);
        }
        if (acq20_find_result == 0) {
            acq20_write_result = fat_overwrite_existing_file(
                &fat_volume,
                &acq20_file,
                &capture_buffer[0][0],
                ACQ_TEST_BANKS * CAPTURE_SIZE);
        }
    }

    console_clear();
    console_putln("RAW8: 1 PREPARE x 8 BANKS");
    put_result("INIT          ", init_ok);
    put_result("TRIGGER APPLY ", trigger_ok);
    put_result("ALL 8 BANKS   ",
        test_ok && completed_cycles == ACQ_TEST_CYCLES &&
        cycle_bank_ok[0] == 0xFFU);
    console_puts("0A/POLLS ");
    put_hex16(cycle_ready_first[0]);
    console_puts(">");
    put_hex16(cycle_ready_final[0]);
    console_puts("/");
    put_hex16(cycle_ready_polls[0]);
    console_putln("");
    console_puts("PTR V/14 ");
    put_hex16(cycle_pointer_valid[0]);
    console_puts("/");
    put_hex16(cycle_live_pointer[0]);
    console_putln("");
    console_puts("290/63C ");
    put_hex16(cycle_runtime_290[0]);
    console_puts("/");
    put_hex16(cycle_runtime_63c[0]);
    console_putln("");
    console_puts("POS20   ");
    put_hex16(cycle_capture_position[0]);
    console_putln("");
    console_puts("MASK ");
    put_hex16(cycle_bank_ok[0]);
    console_puts(" SAVE ");
    put_hex32((uint32_t)acq20_write_result);
    console_putln("");

    for (bank = 0U; bank < ACQ_TEST_BANKS; ++bank) {
        console_puts("B"); put_hex8(bank);
        console_puts(" H"); put_hex32(cycle_hash[0][bank]);
        console_puts(" C"); put_hex16(cycle_changed[0][bank]);
        console_puts(" N"); put_hex16(cycle_nonzero[0][bank]);
        console_puts(" D"); put_hex16(cycle_diff_b0[0][bank]);
        console_putln("");
    }

    print_raw8_transpose(RAW8_TRANSPOSE_ROWS);
    console_present();

    for (;;)
        __asm__ volatile ("wfi");
}
