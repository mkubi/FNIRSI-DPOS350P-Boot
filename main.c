#include <stdint.h>
#include "display.h"
#include "console.h"
#include "custom.h"
#include "start.h"

#define BLACK  0x0000U
#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define CYAN   0x07FFU

static char hexn(uint8_t value)
{
    return value < 10U ? (char)('0' + value)
                       : (char)('A' + value - 10U);
}

static void hex32(uint32_t value)
{
    unsigned i;
    char text[9];

    for (i = 0U; i < 8U; i++)
        text[i] = hexn((uint8_t)((value >> (28U - 4U * i)) & 15U));
    text[8] = 0;
    console_puts(text);
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

static void short_delay(void)
{
    volatile uint32_t count;

    for (count = 0U; count < 4000000U; count++)
        __asm__ volatile ("nop");
}

int main(void)
{
    custom_app_t app;
    int probe_result;
    int load_result = LOADER_ERR_NOT_LOADED;

    display_init();
    console_init(WHITE, BLACK, 1U);
    console_set_color(CYAN);
    console_putln("DPOS350P CUSTOM APP HANDOFF");
    console_set_color(WHITE);
    console_putln("Target: DPOSAPP.BIN -> 0x40000000");
    console_present();

    probe_result = custom_app_probe(&app);
    if (!probe_result)
        load_result = custom_app_load(&app);

    console_clear();
    console_set_color(CYAN);
    console_putln("DPOS350P CUSTOM APP HANDOFF");
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

    if (probe_result || load_result) {
        console_status("HANDOFF ...........", "BLOCKED", RED);
        console_present();
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    value_line("FILE SIZE ..........", app.image.image_size, CYAN);
    value_line("BYTES LOADED .......", app.image.bytes_loaded, GREEN);
    value_line("LOAD ADDRESS .......", app.image.load_address, CYAN);
    value_line("ENTRY ADDRESS ......", app.image.entry_address, CYAN);
    value_line("FIRST ARM WORD .....", app.image.first_word, GREEN);

    if (!loader_image_ready(&app.image)) {
        console_status("IMAGE READY .......", "FAIL", RED);
        console_present();
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    console_status("IMAGE READY .......", "OK", GREEN);
    console_status("HANDOFF ...........", "STARTING", YELLOW);
    console_present();

    /* Keep the final checkpoint visible briefly before DPOSAPP takes over. */
    short_delay();
    loader_start_image(&app.image);
}
