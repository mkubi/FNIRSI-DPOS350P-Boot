#include <stdint.h>

#include "display.h"
#include "console.h"
#include "gt911.h"
#include "timer.h"

#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define BLACK  0x0000U

#define TIMER_TICKS_PER_US     24U
#define POLL_INTERVAL_MS       10U
#define DRAW_INTERVAL_MS       250U
#define POLL_ONLY_DURATION_MS  5000U

struct diag_stats {
    uint32_t polls;
    uint32_t errors;
    uint32_t ready_events;
    uint32_t touch_events;
    uint32_t clear_events;
    uint32_t last_poll_us;
    uint32_t max_poll_us;
    uint32_t last_draw_ms;
    uint32_t max_draw_ms;
    uint8_t raw_status;
    uint8_t int_level;
    uint8_t data_ready;
    uint8_t points;
    uint16_t x;
    uint16_t y;
    int last_result;
};

static char hex_digit(uint32_t value)
{
    value &= 0xFU;
    return value < 10U ? (char)('0' + value) :
                         (char)('A' + value - 10U);
}

static void put_hex32(uint32_t value)
{
    char text[11];
    uint32_t i;

    text[0] = '0';
    text[1] = 'x';
    for (i = 0U; i < 8U; ++i)
        text[2U + i] = hex_digit(value >> (28U - 4U * i));
    text[10] = '\0';
    console_puts(text);
}

static void put_dec(uint32_t value)
{
    char text[11];
    uint32_t i = sizeof(text) - 1U;

    text[i] = '\0';
    do {
        text[--i] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    console_puts(&text[i]);
}

static void line_dec(const char *label, uint32_t value)
{
    console_puts(label);
    put_dec(value);
    console_putln("");
}

static void line_hex(const char *label, uint32_t value)
{
    console_puts(label);
    put_hex32(value);
    console_putln("");
}

static void result_line(const char *label, int pass)
{
    console_puts(label);
    console_set_color(pass ? GREEN : RED);
    console_putln(pass ? "PASS" : "FAIL");
    console_set_color(WHITE);
}

static uint32_t elapsed_us(uint32_t start, uint32_t end)
{
    return (end - start) / TIMER_TICKS_PER_US;
}

static void poll_touch(struct diag_stats *stats)
{
    gt911_debug_sample_t sample;
    uint32_t start;
    uint32_t end;
    uint32_t duration;
    int rc;

    start = timer_ticks();
    rc = gt911_debug_sample(&sample, 1);
    end = timer_ticks();

    duration = elapsed_us(start, end);
    stats->last_poll_us = duration;
    if (duration > stats->max_poll_us)
        stats->max_poll_us = duration;

    ++stats->polls;
    stats->last_result = rc;
    if (rc < 0) {
        ++stats->errors;
        return;
    }

    stats->raw_status = sample.raw_status;
    stats->int_level = sample.int_level;
    stats->data_ready = sample.data_ready;
    stats->points = sample.touch.points;

    if (sample.data_ready) {
        ++stats->ready_events;
        ++stats->clear_events;
    }

    if (sample.touch.pressed) {
        ++stats->touch_events;
        stats->x = sample.touch.x;
        stats->y = sample.touch.y;
    }
}

static void draw_screen(const gt911_info_t *info,
                        const struct diag_stats *stats,
                        int init_ok,
                        int probe_ok,
                        int poll_only)
{
    uint32_t start;
    uint32_t end;
    uint32_t duration_ms;
    struct diag_stats *mutable_stats = (struct diag_stats *)stats;

    start = timer_ticks();

    console_clear();
    console_set_color(YELLOW);
    console_putln("GT911 DIAGNOSTIC");
    console_set_color(WHITE);
    console_putln("----------------");
    result_line("INIT          ", init_ok);
    result_line("PROBE         ", probe_ok);
    console_puts("PRODUCT ID    ");
    console_putln(probe_ok ? info->product_id : "----");
    line_hex("ADDRESS       ", info->address);
    console_puts("PHASE         ");
    console_putln(poll_only ? "POLL ONLY" : "POLL + DRAW");
    console_putln("");

    line_dec("POLL COUNT    ", stats->polls);
    line_dec("POLL ERRORS   ", stats->errors);
    line_dec("READY EVENTS  ", stats->ready_events);
    line_dec("TOUCH EVENTS  ", stats->touch_events);
    line_dec("STATUS CLEARS ", stats->clear_events);
    line_hex("LAST RESULT   ", (uint32_t)stats->last_result);
    console_putln("");

    line_hex("RAW STATUS    ", stats->raw_status);
    line_dec("DATA READY    ", stats->data_ready);
    line_dec("POINTS        ", stats->points);
    line_dec("X             ", stats->x);
    line_dec("Y             ", stats->y);
    line_dec("INT LEVEL     ", stats->int_level);
    console_putln("");

    line_dec("POLL LAST US  ", stats->last_poll_us);
    line_dec("POLL MAX US   ", stats->max_poll_us);
    line_dec("DRAW LAST MS  ", stats->last_draw_ms);
    line_dec("DRAW MAX MS   ", stats->max_draw_ms);

    console_present();

    end = timer_ticks();
    duration_ms = (end - start) / 24000U;
    mutable_stats->last_draw_ms = duration_ms;
    if (duration_ms > mutable_stats->max_draw_ms)
        mutable_stats->max_draw_ms = duration_ms;
}

static void checkpoint(const char *line1, const char *line2)
{
    console_clear();
    console_set_color(YELLOW);
    console_putln("GT911 DIAGNOSTIC");
    console_set_color(WHITE);
    console_putln("----------------");
    console_putln(line1);
    if (line2 != 0)
        console_putln(line2);
    console_present();
}

int main(void)
{
    gt911_info_t info;
    struct diag_stats stats;
    uint32_t next_poll;
    uint32_t next_draw;
    uint32_t now;
    int init_ok;
    int probe_ok;

    stats.polls = 0U;
    stats.errors = 0U;
    stats.ready_events = 0U;
    stats.touch_events = 0U;
    stats.clear_events = 0U;
    stats.last_poll_us = 0U;
    stats.max_poll_us = 0U;
    stats.last_draw_ms = 0U;
    stats.max_draw_ms = 0U;
    stats.raw_status = 0U;
    stats.int_level = 0U;
    stats.data_ready = 0U;
    stats.points = 0U;
    stats.x = 0U;
    stats.y = 0U;
    stats.last_result = 0;

    display_init();
    console_init(WHITE, BLACK, 1U);
    timer_init();

    info.product_id[0] = '\0';
    info.address = 0x5DU;
    info.available = 0U;

    checkpoint("INITIALIZING GT911", 0);
    init_ok = gt911_init() == 0;

    checkpoint(init_ok ? "INIT PASS" : "INIT FAIL",
               "PROBING PRODUCT ID");
    probe_ok = init_ok && gt911_probe(&info) == 0;

    checkpoint(probe_ok ? "PROBE PASS" : "PROBE FAIL",
               "TIMER TEST - WAIT 1 SECOND");
    timer_delay_ms(1000U);

    checkpoint("TIMER PASS", "STARTING FIRST POLL");
    if (probe_ok)
        poll_touch(&stats);

    checkpoint("FIRST POLL RETURNED", "STARTING LIVE TEST");
    timer_delay_ms(500U);

    draw_screen(&info, &stats, init_ok, probe_ok, 0);

    now = timer_get_ms();
    next_poll = now + POLL_INTERVAL_MS;
    next_draw = now + DRAW_INTERVAL_MS;

    for (;;) {
        now = timer_get_ms();

        if ((int32_t)(now - next_poll) >= 0) {
            if (probe_ok)
                poll_touch(&stats);

            /* Do not burst through missed polls after a slow display update. */
            next_poll = timer_get_ms() + POLL_INTERVAL_MS;
        }

        now = timer_get_ms();
        if ((int32_t)(now - next_draw) >= 0) {
            draw_screen(&info, &stats, init_ok, probe_ok, 0);

            /* Schedule from the end of the display update. */
            next_draw = timer_get_ms() + DRAW_INTERVAL_MS;
        }
    }
}
