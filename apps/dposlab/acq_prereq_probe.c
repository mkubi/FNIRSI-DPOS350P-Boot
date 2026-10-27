#include <stdint.h>
#include "acq_prereq_probe.h"
#include "../../ui/console.h"
#include "custom_fpga_start.h"
#include "smec_matrix.h"

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
    const struct dposlab_acq_probe_result *a = &dposlab_acq_probe_result;
    const volatile struct dposlab_smec_matrix_result *s =
        &dposlab_smec_matrix_result;

    console_init(WHITE, BLACK, 2U);
    console_clear();
    console_putln("ACCUMULATED CUSTOM FPGA+SMEC");
    console_putln("-------------------------");
    console_puts("S1 STATE     "); console_putln(dposlab_smec_impl_name(s->s1));
    console_puts("S2 LOCAL     "); console_putln(dposlab_smec_impl_name(s->s2));
    console_puts("S3 PREAMBLE  "); console_putln(dposlab_smec_impl_name(s->s3));
    console_puts("S4 JOBS      "); console_putln(dposlab_smec_impl_name(s->s4));
    console_putln("");
    console_puts("SMEC PASS    "); console_putln(s->success ? "YES" : "NO");
    console_puts("JOBS DONE    "); put_hex(s->custom.completed_jobs); console_putln("");
    console_puts("CURRENT JOB  "); put_hex(s->custom.current_job); console_putln("");
    console_puts("ATTEMPTS     "); put_hex(s->custom.attempts); console_putln("");
    console_puts("POLL64       "); put_hex(s->custom.poll64_count); console_putln("");
    console_puts("POLL65       "); put_hex(s->custom.poll65_count); console_putln("");
    console_puts("RESULT66     "); put_hex(s->custom.result66); console_putln("");
    console_puts("ECHO/STATUS  "); put_hex(((uint32_t)s->custom.echo68 << 8) | s->custom.status67); console_putln("");
    console_puts("ERROR        "); put_hex((uint32_t)s->custom.error); console_putln("");
    console_puts("STATE HASH   "); put_hex(s->state_checksum); console_putln("");
    console_puts("STATE NZ     "); put_hex(s->state_nonzero); console_putln("");
    console_putln("");
    console_puts("0A PRE       "); put_hex(a->raw0a); console_putln("");
    console_puts("0A POST      "); put_hex(a->prepared0a); console_putln("");
    console_puts("READY        "); console_putln(a->ready ? "YES" : "NO");
    console_puts("POLLS        "); put_hex(a->polls); console_putln("");
    console_puts("PE CFG0      "); put_hex(a->pe_cfg0); console_putln("");
    console_puts("PE CFG1      "); put_hex(a->pe_cfg1); console_putln("");
    console_puts("PE DATA      "); put_hex(a->pe_data); console_putln("");
    console_present();
}
