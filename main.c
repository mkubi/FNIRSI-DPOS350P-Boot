#include <stdint.h>
#include "display.h"
#include "console.h"
#include "original.h"
#include "start.h"

#define BLACK  0x0000U
#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define CYAN   0x07FFU

/* Change only this line to test the other known-good original image. */
#define TEST_ORIGINAL_IMAGE ORIGINAL_IMAGE_EXE1_USB
/* #define TEST_ORIGINAL_IMAGE ORIGINAL_IMAGE_EXE1_USB */

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
    original_app_t app;
    int probe_result;
    int load_result = LOADER_ERR_NOT_LOADED;

    display_init();
    console_init(WHITE, BLACK, 1U);
    console_set_color(CYAN);
    console_putln("DPOS350P ORIGINAL IMAGE TEST");
    console_set_color(WHITE);
    console_puts("Target: ");
    console_putln(original_image_name(TEST_ORIGINAL_IMAGE));
    console_present();

    probe_result = original_app_probe(TEST_ORIGINAL_IMAGE, &app);
    if (!probe_result)
        load_result = original_app_load(&app);

    console_clear();
    console_set_color(CYAN);
    console_putln("DPOS350P ORIGINAL IMAGE TEST");
    console_putln("");
    console_set_color(WHITE);
    console_puts("TARGET ............. ");
    console_set_color(CYAN);
    console_putln(original_image_name(TEST_ORIGINAL_IMAGE));

    console_status("EGON HEADER .......",
                   probe_result ? "FAIL" : "OK",
                   probe_result ? RED : GREEN);
    console_status("PAYLOAD LOAD ......",
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

    value_line("HEADER OFFSET ......", app.header_offset, CYAN);
    value_line("PAYLOAD OFFSET .....", app.payload_offset, CYAN);
    value_line("TOTAL SIZE .........", app.total_size, CYAN);
    value_line("PAYLOAD SIZE .......", app.image.image_size, CYAN);
    value_line("LOAD ADDRESS .......", app.image.load_address, CYAN);
    value_line("FIRST ARM WORD .....", app.image.first_word, GREEN);

    if (!loader_image_ready(&app.image)) {
        console_status("IMAGE READY .......", "FAIL", RED);
        console_present();
        for (;;) {
            __asm__ volatile ("wfi");
        }
    }

    console_status("HEADER STRIPPED ...", "YES", GREEN);
    console_status("IMAGE READY .......", "OK", GREEN);
    console_status("HANDOFF ...........", "STARTING", YELLOW);
    console_present();

    short_delay();
    loader_start_image(&app.image);
}
