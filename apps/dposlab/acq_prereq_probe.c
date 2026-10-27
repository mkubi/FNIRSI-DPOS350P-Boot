#include <stdint.h>
#include "acq_prereq_probe.h"
#include "../../ui/console.h"
#include "custom_fpga_start.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U

#define PE_CFG0 (*(volatile uint32_t *)0x01C20890U)
#define PE_CFG1 (*(volatile uint32_t *)0x01C20894U)
#define PE_DATA (*(volatile uint32_t *)0x01C208A0U)
#define STOCK_ACQ_MODE_BYTE (*(volatile uint8_t *)0x40545A1BU)
#define FACTORY_MAGIC_BASE ((volatile uint32_t *)0x42E68AE0U)

#define STOCK_FPGA_READ_BYTE \
    ((uint8_t (*)(void))(uintptr_t)0x400491B8U)
#define STOCK_FPGA_SELECT \
    ((void (*)(uint32_t))(uintptr_t)0x4004927CU)
#define STOCK_ACQ_PREPARE \
    ((void (*)(void))(uintptr_t)0x4003FA10U)

struct dposlab_acq_probe_result dposlab_acq_probe_result
    __attribute__((section(".data")));

static void put_hex(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[11];
    uint32_t i;

    text[0] = '0';
    text[1] = 'x';
    for (i = 0U; i < 8U; ++i)
        text[2U + i] = hex[(value >> (28U - i * 4U)) & 15U];
    text[10] = 0;
    console_puts(text);
}

static uint8_t stock_read_selector(uint8_t selector)
{
    STOCK_FPGA_SELECT((uint32_t)selector);
    return STOCK_FPGA_READ_BYTE();
}

void dposlab_acq_variant1_probe(void)
{
    struct dposlab_acq_probe_result *r = &dposlab_acq_probe_result;
    uint32_t i;
    uint8_t value = 0U;

    r->factory_magic0 = FACTORY_MAGIC_BASE[0];
    r->factory_magic1 = FACTORY_MAGIC_BASE[1];
    r->factory_magic2 = FACTORY_MAGIC_BASE[2];
    r->factory_magic3 = FACTORY_MAGIC_BASE[3];
    r->mode_byte = STOCK_ACQ_MODE_BYTE;

    r->raw0a = stock_read_selector(0x0AU);
    r->prepared0a = 0U;
    r->polls = 0U;
    r->ready = 0U;

    STOCK_ACQ_PREPARE();
    for (i = 0U; i < 2500U; ++i) {
        value = stock_read_selector(0x0AU);
        r->prepared0a = value;
        r->polls = i + 1U;
        if ((value & 1U) != 0U) {
            r->ready = 1U;
            break;
        }
    }

    r->pe_cfg0 = PE_CFG0;
    r->pe_cfg1 = PE_CFG1;
    r->pe_data = PE_DATA;
}

void dposlab_acq_variant1_monitor(void)
{
    const struct dposlab_acq_probe_result *r = &dposlab_acq_probe_result;
    const volatile struct dposlab_custom_fpga_start_result *f =
        &dposlab_custom_fpga_start_result;

    console_init(WHITE, BLACK, 2U);
    console_clear();
    console_putln("FPGA_START MATRIX TESTBENCH");
    console_putln("---------------------------");
    console_puts("F1 CSI       "); console_putln(dposlab_fpga_impl_name(f->f1));
    console_puts("F2 STATE     "); console_putln(dposlab_fpga_impl_name(f->f2));
    console_puts("F3 GPIO      "); console_putln(dposlab_fpga_impl_name(f->f3));
    console_puts("F4 CLOCK     "); console_putln(dposlab_fpga_impl_name(f->f4));
    console_putln("");
    console_puts("ID           "); console_putln(f->id_ok ? "PASS" : "FAIL");
    console_puts("CSI PHASE    "); console_putln(f->csi_ok ? "PASS" : "FAIL");
    console_puts("CLOCK PHASE  "); console_putln(f->clock_ok ? "PASS" : "FAIL");
    console_puts("IF MODE      "); put_hex(f->interface_mode_state); console_putln("");
    console_puts("BUS DIR      "); put_hex(f->bus_direction_state); console_putln("");
    console_puts("CSI IF       "); put_hex(f->csi_if_cfg); console_putln("");
    console_puts("CSI MODE     "); put_hex(f->csi_mode); console_putln("");
    console_puts("CSI HSIZE    "); put_hex(f->csi_hsize); console_putln("");
    console_puts("CSI BUF0     "); put_hex(f->csi_buffer0); console_putln("");
    console_puts("CSI IRQ      "); put_hex(f->csi_irq_enable); console_putln("");
    console_putln("");
    console_puts("ACQ MODE     "); put_hex(r->mode_byte); console_putln("");
    console_puts("0A PRE       "); put_hex(r->raw0a); console_putln("");
    console_puts("0A POST      "); put_hex(r->prepared0a); console_putln("");
    console_puts("READY        "); console_putln(r->ready ? "YES" : "NO");
    console_puts("POLLS        "); put_hex(r->polls); console_putln("");
    console_puts("PE CFG0      "); put_hex(r->pe_cfg0); console_putln("");
    console_puts("PE CFG1      "); put_hex(r->pe_cfg1); console_putln("");
    console_puts("PE DATA      "); put_hex(r->pe_data); console_putln("");
    console_present();
}
