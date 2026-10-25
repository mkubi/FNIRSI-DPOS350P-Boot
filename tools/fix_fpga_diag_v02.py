#!/usr/bin/env python3
from pathlib import Path

root = Path('.')
ui = root / 'apps/fpga_diag/ui_adapter.c'
drv = root / 'drivers/fpga_bus_ro.c'

ui.write_text(r'''#include <stdint.h>
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

void diag_ui_hex8x4(const char *label, const uint8_t value[4])
{
    char out[12];
    int i;

    for (i = 0; i < 4; ++i) {
        out[i * 3 + 0] = hex_digit(value[i] >> 4);
        out[i * 3 + 1] = hex_digit(value[i]);
        if (i != 3)
            out[i * 3 + 2] = ' ';
    }
    out[11] = '\0';

    console_puts(label);
    console_putln(out);
    console_present();
}
''')

text = drv.read_text()
text = text.replace(
'''    /* Conservative idle state reconstructed from EXE2 helpers. */
    PE_DATA = (PE_DATA & ~PE_CTRL_MASK) | 0x0000000Fu;
    barrier();''',
'''    /* EXE2 0x40049110 idle state: PE0 high, PE1 low. */
    PE_DATA = (PE_DATA & ~PE_CTRL_MASK) | 0x00000001u;
    barrier();''')
text = text.replace(
'''    /* EXE2 0x4004927C path: PE2 low, PE3 low, byte on PE8..15,
       pulse PE0 low then high. */
    pe_clear(2);
    pe_clear(3);''',
'''    /* EXE2 0x4004927C path: PE2 high, PE3 high, byte on PE8..15,
       pulse PE0 low then high. */
    pe_set(2);
    pe_set(3);''')
text = text.replace(
'''    /* Leave the data bus as input and strobes high. */
    data_bus_input();
    pe_set(0);
    pe_set(1);
    pe_set(2);
    pe_set(3);''',
'''    /* Restore the original bus-idle convention: data input, PE0 high,
       PE1 low. PE2/PE3 are deasserted high after the transaction. */
    data_bus_input();
    pe_set(0);
    pe_clear(1);
    pe_set(2);
    pe_set(3);''')
drv.write_text(text)
print('Patched FPGA diagnostic v0.2 UI and bus polarity')
