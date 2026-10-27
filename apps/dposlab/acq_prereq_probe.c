#include <stdint.h>
#include "acq_prereq_probe.h"
#include "../../ui/console.h"

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

    console_init(WHITE, BLACK, 2U);
    console_clear();
    console_putln("DPOSLAB STOCK MAIN PRE-WINOPEN");
    console_putln("-----------------------------");
    console_putln("STOCK MAIN EARLY FLOW        PASS");
    console_putln("GUI/GT911/WIN/COLOR/FAT      NOP");
    console_putln("FactoryCheck                 PASS");
    console_putln("SysConfigExec                PASS");
    console_putln("WIN_OpenMainBank             NOT RUN");
    console_putln("");
    console_puts("MAGIC +00                    ");
    put_hex(r->factory_magic0);
    console_putln("");
    console_puts("MAGIC +04                    ");
    put_hex(r->factory_magic1);
    console_putln("");
    console_puts("MAGIC +08                    ");
    put_hex(r->factory_magic2);
    console_putln("");
    console_puts("MAGIC +0C                    ");
    put_hex(r->factory_magic3);
    console_putln("");
    console_puts("ACQ MODE BYTE                ");
    put_hex(r->mode_byte);
    console_putln("");
    console_puts("RAW 0x0A BEFORE PREP         ");
    put_hex(r->raw0a);
    console_putln("");
    console_puts("0x0A AFTER STOCK PREP        ");
    put_hex(r->prepared0a);
    console_putln("");
    console_puts("READY                        ");
    console_putln(r->ready ? "YES" : "NO");
    console_puts("POLL COUNT                   ");
    put_hex(r->polls);
    console_putln("");
    console_puts("PE CFG0                      ");
    put_hex(r->pe_cfg0);
    console_putln("");
    console_puts("PE CFG1                      ");
    put_hex(r->pe_cfg1);
    console_putln("");
    console_puts("PE DATA                      ");
    put_hex(r->pe_data);
    console_putln("");
    console_putln("");
    console_putln("CHECKPOINT BEFORE WIN_OPEN");
    console_putln("POWER CYCLE TO EXIT");
    console_present();
}
