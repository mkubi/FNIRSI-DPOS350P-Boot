#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"
#include "sdmmc_test.h"
#include "crc32.h"

#define WHITE  0xFFFFU
#define BLACK  0x0000U
#define GREEN  0x07E0U
#define RED    0xF800U
#define CYAN   0x07FFU
#define YELLOW 0xFFE0U

#define BENCH_START_LBA      0U
#define BENCH_SECTOR_COUNT   1024U
#define BENCH_SECTOR_SIZE    512U
#define BENCH_BUFFER_ADDRESS 0x42000000U
#define BENCH_BUFFER_BYTES   (BENCH_SECTOR_COUNT * BENCH_SECTOR_SIZE)
#define BENCH_CLOCK_HZ       24000000U
#define STRESS_RUNS          100U

static char hex_digit(uint8_t value)
{
    return value < 10U ? (char)('0' + value)
                       : (char)('A' + value - 10U);
}

static void put_hex32(uint32_t value)
{
    char text[9];
    unsigned i;
    for (i = 0U; i < 8U; ++i)
        text[i] = hex_digit((uint8_t)((value >> (28U - 4U * i)) & 0xFU));
    text[8] = 0;
    console_puts(text);
}

static void put_u32(uint32_t value)
{
    char text[11];
    unsigned pos = sizeof(text) - 1U;
    text[pos] = 0;
    do {
        text[--pos] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value && pos > 0U);
    console_puts(&text[pos]);
}

static void line_u32(const char *label, uint32_t value)
{
    console_set_color(WHITE);
    console_puts(label);
    console_set_color(CYAN);
    put_u32(value);
    console_putln("");
}

static void line_hex(const char *label, uint32_t value)
{
    console_set_color(WHITE);
    console_puts(label);
    console_set_color(CYAN);
    console_puts("0x");
    put_hex32(value);
    console_putln("");
}

static uint32_t rate_kib_s(uint32_t elapsed_ms)
{
    if (!elapsed_ms)
        return 0U;
    return (512U * 1000U) / elapsed_ms;
}

static int read_pass(uint8_t *buffer, uint32_t *elapsed_ms,
                     uint32_t *crc_out, uint32_t *failed_sector,
                     int measure)
{
    uint32_t start = 0U;
    uint32_t end = 0U;
    uint32_t sector;
    uint32_t crc = 0xFFFFFFFFU;
    int r;

    if (measure)
        start = timer_get_ms();

    for (sector = 0U; sector < BENCH_SECTOR_COUNT; ++sector) {
        r = sdmmc0_read_sector(BENCH_START_LBA + sector,
                               buffer + sector * BENCH_SECTOR_SIZE);
        if (r) {
            *failed_sector = sector;
            return r;
        }
    }

    if (measure)
        end = timer_get_ms();

    for (sector = 0U; sector < BENCH_SECTOR_COUNT; ++sector)
        crc = sd_bench_crc32_update(crc,
                                    buffer + sector * BENCH_SECTOR_SIZE,
                                    BENCH_SECTOR_SIZE);

    *elapsed_ms = measure ? end - start : 0U;
    *crc_out = crc ^ 0xFFFFFFFFU;
    *failed_sector = 0xFFFFFFFFU;
    return 0;
}

static void draw_header(void)
{
    console_clear();
    console_set_color(CYAN);
    console_putln("DPOS350P SD BENCHMARK v0.3");
    console_set_color(WHITE);
    console_putln("24 MHz / CMD17 / PIO / 1-BIT");
    console_putln("READ-ONLY STRESS TEST");
    console_putln("");
}

void main(void)
{
    static sdmmc0_info_t card;
    uint8_t *buffer = (uint8_t *)(uintptr_t)BENCH_BUFFER_ADDRESS;
    uint32_t actual_hz = 0U;
    uint32_t reference_crc = 0U;
    uint32_t elapsed = 0U;
    uint32_t crc = 0U;
    uint32_t failed_sector = 0xFFFFFFFFU;
    uint32_t min_ms = 0xFFFFFFFFU;
    uint32_t max_ms = 0U;
    uint32_t total_ms = 0U;
    uint32_t pass_count = 0U;
    uint32_t fail_count = 0U;
    uint32_t crc_fail_count = 0U;
    uint32_t run;
    int r;

    display_init();
    console_init(WHITE, BLACK, 2U);
    timer_init();
    draw_header();
    console_putln("INITIALIZING CARD...");
    console_present();

    r = sdmmc0_init(&card);
    if (!r)
        r = sdmmc0_set_clock_hz(BENCH_CLOCK_HZ, &actual_hz);
    if (r) {
        console_set_color(RED);
        console_putln("INIT/CLOCK: FAIL");
        line_hex("ERROR:      ", (uint32_t)r);
        console_present();
        for (;;) __asm__ volatile ("wfi");
    }

    /* Full unmeasured reference pass. It also absorbs the first timer anomaly. */
    console_set_color(YELLOW);
    console_putln("REFERENCE PASS...");
    console_present();
    r = read_pass(buffer, &elapsed, &reference_crc, &failed_sector, 0);
    if (r) {
        console_set_color(RED);
        console_putln("REFERENCE: FAIL");
        line_hex("ERROR:      ", (uint32_t)r);
        line_u32("SECTOR:     ", failed_sector);
        console_present();
        for (;;) __asm__ volatile ("wfi");
    }

    timer_delay_ms(20U);

    for (run = 0U; run < STRESS_RUNS; ++run) {
        r = read_pass(buffer, &elapsed, &crc, &failed_sector, 1);
        if (r) {
            fail_count++;
            break;
        }
        if (crc != reference_crc) {
            crc_fail_count++;
            break;
        }

        pass_count++;
        total_ms += elapsed;
        if (elapsed < min_ms)
            min_ms = elapsed;
        if (elapsed > max_ms)
            max_ms = elapsed;

        if (((run + 1U) % 10U) == 0U || run == 0U) {
            draw_header();
            line_u32("CLOCK HZ:   ", actual_hz);
            line_hex("REF CRC32:  ", reference_crc);
            line_u32("RUN:        ", run + 1U);
            line_u32("PASS:       ", pass_count);
            line_u32("LAST MS:    ", elapsed);
            line_u32("LAST KiB/s: ", rate_kib_s(elapsed));
            console_set_color(GREEN);
            console_putln("STATUS: PASS");
            console_present();
        }
    }

    draw_header();
    line_u32("CLOCK HZ:   ", actual_hz);
    line_hex("REF CRC32:  ", reference_crc);
    line_u32("RUNS:       ", pass_count + fail_count + crc_fail_count);
    line_u32("PASS:       ", pass_count);
    line_u32("READ FAIL:  ", fail_count);
    line_u32("CRC FAIL:   ", crc_fail_count);

    if (pass_count) {
        line_u32("MIN MS:     ", min_ms);
        line_u32("MAX MS:     ", max_ms);
        line_u32("AVG MS:     ", total_ms / pass_count);
        line_u32("AVG KiB/s:  ", rate_kib_s(total_ms / pass_count));
    }

    if (r) {
        line_hex("ERROR:      ", (uint32_t)r);
        line_u32("SECTOR:     ", failed_sector);
    } else if (crc_fail_count) {
        line_hex("BAD CRC32:  ", crc);
    }

    console_set_color((pass_count == STRESS_RUNS) ? GREEN : RED);
    console_putln((pass_count == STRESS_RUNS) ? "STRESS: PASS" : "STRESS: FAIL");
    console_set_color(WHITE);
    console_putln("POWER CYCLE TO EXIT");
    console_present();

    for (;;) __asm__ volatile ("wfi");
}
