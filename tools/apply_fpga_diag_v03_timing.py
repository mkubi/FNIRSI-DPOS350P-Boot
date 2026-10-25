#!/usr/bin/env python3
from pathlib import Path

h = Path('drivers/fpga_bus_ro.h')
c = Path('drivers/fpga_bus_ro.c')
m = Path('apps/fpga_diag/main.c')
u = Path('apps/fpga_diag/ui_adapter.c')

for path in (h, c, m, u):
    if not path.exists():
        raise SystemExit(f'Missing {path}; run from repository root')

ht = h.read_text()
ht = ht.replace('uint8_t id[4];', 'uint8_t raw[5];')
ht = ht.replace('uint32_t id_word;', 'uint32_t id_word;\n    uint32_t id_word_shifted;')
h.write_text(ht)

ct = c.read_text()
ct = ct.replace(
'''static uint8_t fpga_read_byte(void)
{''',
'''static void fpga_settle_delay(void)
{
    /* Deliberately short bus-settle delay. This is not a millisecond delay.
       The volatile loop prevents -O2 from removing the timing gap. */
    volatile unsigned i;
    for (i = 0; i < 64U; ++i)
        __asm__ volatile ("nop");
    barrier();
}

static uint8_t fpga_read_byte(void)
{''')
ct = ct.replace(
'''    unsigned i;
    uint32_t word = 0;''',
'''    unsigned i;
    uint32_t word = 0;
    uint32_t shifted = 0;''')
ct = ct.replace(
'''    control_bus_init();
    fpga_write_mode1(1u);

    for (i = 0; i < 4; ++i) {
        result->id[i] = fpga_read_byte();
        word = (word << 8) | result->id[i];
    }

    result->id_word = word;
    result->present = word == FPGA_MAGIC;''',
'''    control_bus_init();
    fpga_write_mode1(1u);
    fpga_settle_delay();

    /* Read one extra byte. If the first cycle is a dummy/stale byte, the
       expected magic will appear in raw[1]..raw[4]. */
    for (i = 0; i < 5; ++i)
        result->raw[i] = fpga_read_byte();

    for (i = 0; i < 4; ++i)
        word = (word << 8) | result->raw[i];
    for (i = 1; i < 5; ++i)
        shifted = (shifted << 8) | result->raw[i];

    result->id_word = word;
    result->id_word_shifted = shifted;
    result->present = (word == FPGA_MAGIC) || (shifted == FPGA_MAGIC);''')
c.write_text(ct)

ut = u.read_text()
ut = ut.replace('void diag_ui_hex8x4(const char *label, const uint8_t value[4])',
                'void diag_ui_hex8x5(const char *label, const uint8_t value[5])')
ut = ut.replace('char out[12];', 'char out[15];')
ut = ut.replace('for (i = 0; i < 4; ++i) {', 'for (i = 0; i < 5; ++i) {')
ut = ut.replace("if (i != 3)\n            out[i * 3 + 2] = ' ';", "if (i != 4)\n            out[i * 3 + 2] = ' ';")
ut = ut.replace("out[11] = '\\0';", "out[14] = '\\0';")
u.write_text(ut)

mt = m.read_text()
mt = mt.replace('void diag_ui_hex8x4(const char *label, const uint8_t value[4]);',
                'void diag_ui_hex8x5(const char *label, const uint8_t value[5]);')
mt = mt.replace('diag_ui_hex8x4("RAW ID:         ", result.id);',
'''diag_ui_hex8x5("RAW5:          ", result.raw);
    diag_ui_hex32("ID [0..3]:      ", result.id_word);
    diag_ui_hex32("ID [1..4]:      ", result.id_word_shifted);''')
mt = mt.replace('diag_ui_hex32("ID WORD:        ", result.id_word);\n', '')
m.write_text(mt)

print('Applied FPGA diagnostic v0.3: settle delay + five-byte probe')
