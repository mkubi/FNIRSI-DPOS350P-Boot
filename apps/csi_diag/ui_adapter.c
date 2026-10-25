#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"
#include "fpga_bus_ro.h"

#define BLACK 0x0000U
#define WHITE 0xFFFFU

void diag_ui_init(void)
{
    display_init();
    console_init(WHITE, BLACK, 1U);
    timer_init();
}

void diag_ui_clear(void)
{
    console_clear();
    console_present();
}

void diag_ui_puts(const char *s)
{
    console_puts(s);
    console_present();
}

static char hex_digit(unsigned value)
{
    value &= 0xFU;
    return value < 10U ? (char)('0' + value)
                       : (char)('A' + value - 10U);
}

void diag_ui_hex32(const char *label, uint32_t value)
{
    char out[11];
    int i;

    out[0] = '0';
    out[1] = 'x';
    for (i = 0; i < 8; ++i)
        out[2 + i] = hex_digit(value >> (28U - 4U * (unsigned)i));
    out[10] = '\0';

    console_puts(label);
    console_putln(out);
    console_present();
}

void diag_ui_hex8x5(const char *label, const uint8_t value[5])
{
    char out[15];
    int i;

    for (i = 0; i < 5; ++i) {
        out[i * 3 + 0] = hex_digit(value[i] >> 4);
        out[i * 3 + 1] = hex_digit(value[i]);
        if (i != 4)
            out[i * 3 + 2] = ' ';
    }
    out[14] = '\0';

    console_puts(label);
    console_putln(out);
    console_present();
}

void diag_ui_bool(const char *label, int value)
{
    console_puts(label);
    console_putln(value ? "YES" : "NO");
    console_present();
}


void diag_ui_set_cursor(unsigned int column, unsigned int row)
{
    console_set_cursor(column, row);
}

void diag_ui_text_at(unsigned int column, unsigned int row, const char *text)
{
    console_set_cursor(column, row);
    console_puts(text);
    console_present();
}

void diag_ui_hex32_at(unsigned int column, unsigned int row,
                      const char *label, uint32_t value)
{
    console_set_cursor(column, row);
    diag_ui_hex32(label, value);
}

void diag_ui_bool_at(unsigned int column, unsigned int row,
                     const char *label, int value)
{
    console_set_cursor(column, row);
    diag_ui_bool(label, value);
}

void diag_ui_layout_test(void)
{
    unsigned int i;
    uint32_t left;
    uint32_t right;
    char left_label[] = "L00: ";
    char right_label[] = "R00: ";

    console_clear();
    diag_ui_text_at(2U, 1U, "CSI TWO COLUMN LAYOUT TEST");
    diag_ui_text_at(2U, 3U, "LEFT COLUMN X=2");
    diag_ui_text_at(66U, 3U, "RIGHT COLUMN X=66");

    for (i = 0U; i < 21U; ++i) {
        left_label[1] = (char)('0' + (i / 10U));
        left_label[2] = (char)('0' + (i % 10U));
        right_label[1] = left_label[1];
        right_label[2] = left_label[2];
        left = i * 0x11111111U;
        right = 0xAAAAAAAAU ^ (i * 0x11111111U);
        diag_ui_hex32_at(2U, 5U + i, left_label, left);
        diag_ui_hex32_at(66U, 5U + i, right_label, right);
    }

    diag_ui_text_at(2U, 28U, "LEFT END MARKER");
    diag_ui_text_at(66U, 28U, "RIGHT END MARKER");
    diag_ui_bool_at(2U, 30U, "CURSOR API:", 1);
    diag_ui_text_at(66U, 30U, "NO FPGA OR CSI ACCESS");
}


void diag_ui_capture_screen(int fpga_ok, int clock_ok, int config_ok,
                            uint32_t buffer_reg, uint32_t status_before,
                            int done, uint32_t status_after,
                            uint32_t mode_after, int left_guard_ok,
                            int right_guard_ok, uint32_t changed,
                            uint32_t first_change, uint32_t last_change,
                            uint32_t data0, uint32_t data1,
                            uint32_t data2, uint32_t data3)
{
    console_clear();
    diag_ui_text_at(2U, 1U, "CSI STAGE 3C BANK0 CAPTURE");
    diag_ui_text_at(2U, 3U, "CONTROL / CSI");
    diag_ui_text_at(66U, 3U, "BUFFER ANALYSIS");

    diag_ui_bool_at(2U, 5U, "FPGA ID:       ", fpga_ok);
    diag_ui_text_at(2U, 6U, "ARM 08:         SENT");
    diag_ui_text_at(2U, 7U, "BANK 15/00:      SENT");
    diag_ui_bool_at(2U, 8U, "CSI CLOCK:     ", clock_ok);
    diag_ui_bool_at(2U, 9U, "CSI CONFIG:    ", config_ok);
    diag_ui_hex32_at(2U, 10U, "BUF0:          ", buffer_reg);
    diag_ui_hex32_at(2U, 11U, "STATUS PRE:    ", status_before);
    diag_ui_bool_at(2U, 12U, "DONE:          ", done);
    diag_ui_bool_at(2U, 13U, "TIMEOUT:       ", !done);
    diag_ui_hex32_at(2U, 14U, "STATUS POST:   ", status_after);
    diag_ui_hex32_at(2U, 15U, "MODE POST:     ", mode_after);

    diag_ui_bool_at(66U, 5U, "LEFT GUARD:   ", left_guard_ok);
    diag_ui_bool_at(66U, 6U, "RIGHT GUARD:  ", right_guard_ok);
    diag_ui_hex32_at(66U, 7U, "CHANGED:      ", changed);
    diag_ui_hex32_at(66U, 8U, "FIRST:        ", first_change);
    diag_ui_hex32_at(66U, 9U, "LAST:         ", last_change);
    diag_ui_hex32_at(66U, 11U, "DATA 00:      ", data0);
    diag_ui_hex32_at(66U, 12U, "DATA 04:      ", data1);
    diag_ui_hex32_at(66U, 13U, "DATA 08:      ", data2);
    diag_ui_hex32_at(66U, 14U, "DATA 0C:      ", data3);

    diag_ui_text_at(2U, 19U, "CAPTURE WAS FORCED OFF AFTER 100 MS MAX");
    diag_ui_text_at(2U, 21U, "POWER CYCLE TO EXIT");
}


void diag_ui_hex8x5_at(unsigned int column, unsigned int row,
                       const char *label, const uint8_t value[5])
{
    console_set_cursor(column, row);
    diag_ui_hex8x5(label, value);
}

void diag_ui_fpga_ab_screen(const struct fpga_diag_result *baseline,
                            const struct fpga_diag_result *stock)
{
    console_clear();
    diag_ui_text_at(2U, 1U, "FPGA COMMAND PRIMITIVE A/B TEST");
    diag_ui_text_at(2U, 3U, "A: VERIFIED BASELINE");
    diag_ui_text_at(66U, 3U, "B: STOCK-DECODED");

    diag_ui_hex8x5_at(2U, 5U, "RAW5: ", baseline->raw);
    diag_ui_hex32_at(2U, 6U, "ID0:  ", baseline->id_word);
    diag_ui_hex32_at(2U, 7U, "ID1:  ", baseline->id_word_shifted);
    diag_ui_bool_at(2U, 8U, "PRESENT:", baseline->present);
    diag_ui_hex32_at(2U, 10U, "PE DATA: ", baseline->pe_data_after);

    diag_ui_hex8x5_at(66U, 5U, "RAW5: ", stock->raw);
    diag_ui_hex32_at(66U, 6U, "ID0:  ", stock->id_word);
    diag_ui_hex32_at(66U, 7U, "ID1:  ", stock->id_word_shifted);
    diag_ui_bool_at(66U, 8U, "PRESENT:", stock->present);
    diag_ui_hex32_at(66U, 10U, "PE DATA: ", stock->pe_data_after);

    diag_ui_text_at(2U, 14U, "NO 0x08, NO 0x15, NO CSI ACCESS");
    diag_ui_text_at(2U, 16U, "POWER CYCLE TO EXIT");
}
