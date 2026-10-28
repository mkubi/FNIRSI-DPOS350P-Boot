#include <stdint.h>
#include "sysconfig_group_matrix.h"
#include "acq_prereq_probe.h"
#include "../../arch/r11/cache.h"

#define STOCK_CAPTURE_BANK0 ((void (*)(uint32_t))(uintptr_t)0x400406C8U)
#define STOCK_CONFIGURE_CSI_STREAM ((void (*)(void))(uintptr_t)0x400405E0U)
#define STOCK_BANK0_ADDRESS      0x42DF76D4U
#define STOCK_BANK0_SIZE         0x00002134U
#define STOCK_BANK0_SENTINEL     0xA5U
#define STOCK_BANK0_NO_CHANGE    0xFFFFFFFFU
#define CSI_WAIT_LIMIT            24000000U
#include "../../ui/console.h"
#include "../../drivers/fpga_bus.h"
#include "../../drivers/r11_csi.h"
#include "../../drivers/dpos_acq.h"
#include "custom_fpga_start.h"
#include "smec_matrix.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U

#define PE_CFG0 (*(volatile uint32_t *)0x01C20890U)
#define PE_CFG1 (*(volatile uint32_t *)0x01C20894U)
#define PE_DATA (*(volatile uint32_t *)0x01C208A0U)
#define STOCK_ACQ_MODE_BYTE (*(volatile uint8_t *)0x40545A1BU)
#define FACTORY_MAGIC_BASE ((volatile uint32_t *)0x42E68AE0U)

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


static void bank0_fill(uint8_t value)
{
    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)STOCK_BANK0_ADDRESS;
    uint32_t i;

    for (i = 0U; i < STOCK_BANK0_SIZE; ++i)
        p[i] = value;
}

static uint32_t bank0_hash(void)
{
    volatile const uint8_t *p =
        (volatile const uint8_t *)(uintptr_t)STOCK_BANK0_ADDRESS;
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0U; i < STOCK_BANK0_SIZE; ++i)
        hash = (hash ^ p[i]) * 16777619U;

    return hash;
}

static uint32_t bank0_load_u32(uint32_t offset)
{
    volatile const uint8_t *p =
        (volatile const uint8_t *)(uintptr_t)(STOCK_BANK0_ADDRESS + offset);

    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void bank0_inspect(struct dposlab_acq_probe_result *r)
{
    volatile const uint8_t *p =
        (volatile const uint8_t *)(uintptr_t)STOCK_BANK0_ADDRESS;
    uint32_t i;

    r->bank0_changed_bytes = 0U;
    r->bank0_first_change = STOCK_BANK0_NO_CHANGE;
    r->bank0_last_change = STOCK_BANK0_NO_CHANGE;

    for (i = 0U; i < STOCK_BANK0_SIZE; ++i) {
        if (p[i] != STOCK_BANK0_SENTINEL) {
            if (r->bank0_changed_bytes == 0U)
                r->bank0_first_change = i;
            r->bank0_last_change = i;
            ++r->bank0_changed_bytes;
        }
    }

    r->bank0_hash_after = bank0_hash();
    r->bank0_data0 = bank0_load_u32(0U);
    r->bank0_data1 = bank0_load_u32(4U);
    r->bank0_data2 = bank0_load_u32(8U);
    r->bank0_data3 = bank0_load_u32(12U);
}

void dposlab_acq_variant1_probe(void)
{
    struct dposlab_acq_probe_result *r = &dposlab_acq_probe_result;
    struct dpos_acq_prepare_result prepare;
    struct dpos_acq_capture_result capture;

    r->factory_magic0 = FACTORY_MAGIC_BASE[0];
    r->factory_magic1 = FACTORY_MAGIC_BASE[1];
    r->factory_magic2 = FACTORY_MAGIC_BASE[2];
    r->factory_magic3 = FACTORY_MAGIC_BASE[3];
    r->mode_byte = STOCK_ACQ_MODE_BYTE;

    r->raw0a = 0U;
    r->prepared0a = 0U;
    r->polls = 0U;
    r->ready = 0U;
    r->capture_called = 0U;
    r->capture_succeeded = 0U;
    r->csi_wait_polls = 0U;
    r->csi_wait_status = 0U;
    r->csi_wait_mode = 0U;
    r->csi_wait_buffer0 = 0U;
    r->csi_wait_irq_enable = 0U;
    r->csi_wait_hw_done = 0U;
    r->stream_a_640 = 0U;
    r->stream_b_6f2 = 0U;
    r->stream_geometry = 0U;
    r->stream_quantity = 0U;
    r->stream_payload_guess = 0U;

    dpos_acq_prepare(STOCK_ACQ_MODE_BYTE, 2500U, &prepare);
    r->raw0a = prepare.status_before;
    r->prepared0a = prepare.status_after;
    r->polls = prepare.polls;
    r->ready = prepare.ready ? 1U : 0U;

    r->pe_cfg0 = PE_CFG0;
    r->pe_cfg1 = PE_CFG1;
    r->pe_data = PE_DATA;
    if (r->ready != 0U) {
        console_init(WHITE, BLACK, 2U);
        console_clear();
        console_putln("CUSTOM CSI BANK0 C2");
        console_putln("CUSTOM CAPTURE");
        console_putln("WAITING...");
        console_present();

        bank0_fill(STOCK_BANK0_SENTINEL);
        r->bank0_hash_before = bank0_hash();
        if (cache_dcache_enabled())
            cache_clean_range(STOCK_BANK0_ADDRESS, STOCK_BANK0_SIZE);
        cache_dsb();

        /*
         * M1 single-variable experiment:
         *
         * C1A replaces outer stock 0x400405E0 while preserving the deeper
         * C3B replaces 0x40065FA8; C2/C3A replace the Bank0 capture path.
         *
         * Keep every other minimal-acquisition step unchanged and test only
         * whether this missing producer/receiver geometry setup turns the
         * one-byte transfer into a full payload.
         */
        {
            volatile const uint16_t *pa =
                (volatile const uint16_t *)(uintptr_t)(0x40544E68U + 0x640U);
            volatile const uint16_t *pb =
                (volatile const uint16_t *)(uintptr_t)(0x40544E68U + 0x6F2U);
            uint32_t va = *pa;
            uint32_t vb = *pb;
            r->stream_a_640 = va;
            r->stream_b_6f2 = vb;
            r->stream_geometry = (va + (vb << 1)) & 0xFFFFU;
            r->stream_quantity = (20U + ((va + vb) << 1)) & 0xFFFFU;
            r->stream_payload_guess = r->stream_quantity >= 14U ?
                r->stream_quantity - 14U : 0U;
        }
        r->capture_called = 1U;
        r->capture_succeeded = dpos_acq_capture(
            (uintptr_t)STOCK_BANK0_ADDRESS,
            0U,
            0U,
            CSI_WAIT_LIMIT,
            &capture) ? 1U : 0U;

        r->csi_wait_status = capture.status_after;
        r->csi_wait_mode = capture.mode;
        r->csi_wait_buffer0 = capture.buffer0;
        r->csi_wait_irq_enable = capture.irq_enable;
        r->csi_wait_polls = capture.wait_polls;
        r->csi_wait_hw_done = capture.done ? 1U : 0U;

        if (cache_dcache_enabled())
            cache_invalidate_range(STOCK_BANK0_ADDRESS, STOCK_BANK0_SIZE);
        cache_dsb();
        bank0_inspect(r);

        console_putln("CUSTOM CAPTURE RETURNED");
        console_present();
    }
}

void dposlab_acq_variant1_monitor(void)
{
    const struct dposlab_acq_probe_result *a = &dposlab_acq_probe_result;
    const volatile struct dposlab_smec_matrix_result *s =
        &dposlab_smec_matrix_result;

    console_init(WHITE, BLACK, 2U);
    console_clear();
    console_putln("DPOS ACQUISITION REGRESSION");
    console_putln("--------------------------");
    console_puts("SMEC PASS    "); console_putln(s->success ? "YES" : "NO");
    console_puts("JOBS DONE    "); put_hex(s->custom.completed_jobs); console_putln("");
    console_puts("0A PRE       "); put_hex(a->raw0a); console_putln("");
    console_puts("0A POST      "); put_hex(a->prepared0a); console_putln("");
    console_puts("ACQ READY    "); console_putln(a->ready ? "YES" : "NO");
    console_puts("0A POLLS     "); put_hex(a->polls); console_putln("");
    console_putln("");
    console_puts("CAPTURE CALL "); console_putln(a->capture_called ? "YES" : "NO");
    console_puts("CAPTURE PASS "); console_putln(a->capture_succeeded ? "YES" : "NO");
    console_puts("HW DONE      "); console_putln(a->csi_wait_hw_done ? "YES" : "NO");
    console_puts("WAIT POLLS   "); put_hex(a->csi_wait_polls); console_putln("");
    console_puts("CSI STATUS   "); put_hex(a->csi_wait_status); console_putln("");
    console_puts("CSI MODE     "); put_hex(a->csi_wait_mode); console_putln("");
    console_puts("CSI BUF0     "); put_hex(a->csi_wait_buffer0); console_putln("");
    console_puts("CSI IRQ EN   "); put_hex(a->csi_wait_irq_enable); console_putln("");
    console_putln("");
    console_puts("RAM CHANGED  "); console_putln(a->bank0_changed_bytes ? "YES" : "NO");
    console_puts("CHANGED BYTES"); put_hex(a->bank0_changed_bytes); console_putln("");
    console_puts("FIRST CHANGE "); put_hex(a->bank0_first_change); console_putln("");
    console_puts("LAST CHANGE  "); put_hex(a->bank0_last_change); console_putln("");
    console_puts("HASH BEFORE  "); put_hex(a->bank0_hash_before); console_putln("");
    console_puts("HASH AFTER   "); put_hex(a->bank0_hash_after); console_putln("");
    console_puts("DATA 00      "); put_hex(a->bank0_data0); console_putln("");
    console_puts("DATA 04      "); put_hex(a->bank0_data1); console_putln("");
    console_puts("DATA 08      "); put_hex(a->bank0_data2); console_putln("");
    console_puts("DATA 0C      "); put_hex(a->bank0_data3); console_putln("");


    /* T6 inputs and derived values used as the C1A oracle. */
    console_set_cursor(31U, 2U); console_puts("STREAM INPUT");
    console_set_cursor(31U, 3U); console_puts("A640  "); put_hex(a->stream_a_640);
    console_set_cursor(31U, 4U); console_puts("B6F2  "); put_hex(a->stream_b_6f2);
    console_set_cursor(31U, 6U); console_puts("GEOM  "); put_hex(a->stream_geometry);
    console_set_cursor(31U, 7U); console_puts("QTY   "); put_hex(a->stream_quantity);
    console_set_cursor(31U, 8U); console_puts("PAYLD "); put_hex(a->stream_payload_guess);
    console_present();
}
