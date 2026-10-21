#include <stdint.h>
#include "display.h"
#include "console.h"
#include "custom.h"

#define BLACK  0x0000U
#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define CYAN   0x07FFU

static char hexn(uint8_t v)
{
    return v < 10U ? (char)('0' + v) : (char)('A' + v - 10U);
}

static void hex32(uint32_t v)
{
    unsigned i;
    char b[9];

    for (i = 0U; i < 8U; i++)
        b[i] = hexn((uint8_t)((v >> (28U - 4U * i)) & 15U));
    b[8] = 0;
    console_puts(b);
}

static void value_line(const char *name, uint32_t value, uint16_t color)
{
    console_set_color(WHITE);
    console_puts(name);
    console_puts(" ");
    console_set_color(color);
    hex32(value);
    console_putln("");
}

int main(void)
{
    custom_app_t app;
    int probe_result;
    int load_result = LOADER_ERR_NOT_LOADED;

    display_init();
    console_init(WHITE, BLACK, 1U);
    console_set_color(CYAN);
    console_putln("DPOS350P CUSTOM LOADER TEST");
    console_set_color(WHITE);
    console_putln("Target file: DPOSAPP.BIN");
    console_putln("Jump is intentionally disabled");
    console_present();

    probe_result = custom_app_probe(&app);
    if (!probe_result)
        load_result = custom_app_load(&app);

    console_clear();
    console_set_color(CYAN);
    console_putln("DPOS350P CUSTOM LOADER TEST");
    console_putln("");

    console_status("PROBE DPOSAPP.BIN .",
                   probe_result ? "FAIL" : "OK",
                   probe_result ? RED : GREEN);
    console_status("LOAD TO 40000000 .",
                   load_result ? "FAIL" : "OK",
                   load_result ? RED : GREEN);

    value_line("PROBE ERROR ........", (uint32_t)probe_result,
               probe_result ? RED : CYAN);
    value_line("LOAD ERROR .........", (uint32_t)load_result,
               load_result ? RED : CYAN);

    if (!probe_result) {
        console_putln("");
        value_line("FAT TYPE ...........", app.volume.fat_type, CYAN);
        value_line("FILE CLUSTER .......", app.file.first_cluster, CYAN);
        value_line("FILE SIZE ..........", app.file.size, CYAN);
        value_line("LOAD ADDRESS .......", app.image.load_address, CYAN);
        value_line("ENTRY ADDRESS ......", app.image.entry_address, CYAN);
        value_line("SECTORS TO LOAD ....", app.sectors_to_load, CYAN);
    }

    if (!load_result) {
        console_putln("");
        value_line("BYTES LOADED .......", app.image.bytes_loaded, GREEN);
        value_line("FIRST ARM WORD .....", app.image.first_word, GREEN);
        console_status("IMAGE VERIFY ......", "OK", GREEN);
        console_status("JUMP ..............", "DISABLED", YELLOW);
    }

    console_present();
    for (;;) {}
}
