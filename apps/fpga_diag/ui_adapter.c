#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"

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
