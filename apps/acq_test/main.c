#include <stdint.h>

#include "display.h"
#include "dpos_hw.h"
#include "console.h"
#include "timer.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U
#define GREEN 0x07E0U
#define RED   0xF800U

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

static void put_result(const char *label, int pass)
{
    console_puts(label);
    console_set_color(pass ? GREEN : RED);
    console_putln(pass ? "YES" : "NO");
    console_set_color(WHITE);
}

int main(void)
{
    struct dpos_hw_init_result hw;
    int success;

    display_init();
    console_init(WHITE, BLACK, 2U);
    timer_init();
    console_clear();

    console_putln("STANDALONE ACQUISITION TEST");
    console_putln("---------------------------");
    console_putln("R9B FPGA INITIALIZATION");
    console_putln("");
    console_present();

    success = dpos_hw_init(&hw);

    console_puts("FPGA ID PRE   "); put_hex32(hw.fpga_id_before); console_putln("");
    put_result("FPGA PRE      ", hw.fpga_before);
    put_result("CSI INIT      ", hw.csi_ready);
    put_result("CLOCK INIT    ", hw.clocks_ready);
    console_puts("CLOCK WRITES  "); put_hex32(hw.clock_writes); console_putln("");
    console_puts("CSI IF CFG    "); put_hex32(hw.csi_if_cfg); console_putln("");
    console_puts("CSI HSIZE     "); put_hex32(hw.csi_hsize); console_putln("");
    console_puts("CSI BUF0      "); put_hex32(hw.csi_buffer0); console_putln("");
    console_puts("CSI IRQ EN    "); put_hex32(hw.csi_irq_enable); console_putln("");
    console_puts("FPGA ID POST  "); put_hex32(hw.fpga_id_after); console_putln("");
    put_result("FPGA POST     ", hw.fpga_after);
    console_putln("");
    put_result("FPGA INIT     ", success);
    console_putln("");
    console_putln("NO SMEC / NO ACQUISITION");
    console_putln("POWER CYCLE TO EXIT");
    console_present();

    for (;;)
        __asm__ volatile ("wfi");
}
