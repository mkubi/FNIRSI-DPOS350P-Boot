#include <stdint.h>
#include "stock_init_custom_acq.h"
#include "../../arch/r11/cache.h"
#include "../../drivers/acq.h"
#include "../../drivers/fpga_protocol.h"
#include "../../ui/console.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U

#define STOCK_SYS_CONFIG_EXEC     ((void (*)(void))(uintptr_t)0x4003E708U)
#define STOCK_VARIANT_A_SELECT_BLOCK \
    ((void (*)(uint32_t))(uintptr_t)0x4006338CU)
#define STOCK_VARIANT_A_PREPARE \
    ((void (*)(void))(uintptr_t)0x4003FA10U)
#define STOCK_VARIANT_B_UPDATE_POSITION \
    ((void (*)(void))(uintptr_t)0x4003FAE4U)
#define STOCK_VARIANT_B_RUNTIME_MATH \
    ((void (*)(void))(uintptr_t)0x4003FB90U)
#define STOCK_VARIANT_B_WINDOW_BOUNDS \
    ((void (*)(void))(uintptr_t)0x40040030U)
#define STOCK_VARIANT_C_STREAM_CONFIG \
    ((void (*)(void))(uintptr_t)0x400405E0U)
#define STOCK_APP_STATE_BASE      0x40544E68U
#define STOCK_STATE16(offset) \
    (*(volatile uint16_t *)(uintptr_t)(STOCK_APP_STATE_BASE + (offset)))
#define STOCK_STATE32(offset) \
    (*(volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + (offset)))
#define VARIANT_A_READY_POLLS      2500U
#define ORACLE_RAW_BASE           0x42DF76D4U
#define ORACLE_BANK_STRIDE        0x00002134U
#define ORACLE_ACTIVE_SIZE        0x00001800U
#define ORACLE_CAPTURE_TIMEOUT    2000000U
#define ORACLE_RESULT_MARKER      0x4F524143U /* "ORAC" */

volatile struct dposlab_stock_init_custom_acq_result
    dposlab_stock_init_custom_acq_result
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

static uint32_t load_u32(volatile const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void clear_active_bank(uint32_t bank)
{
    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)
        (ORACLE_RAW_BASE + bank * ORACLE_BANK_STRIDE);
    uint32_t i;

    for (i = 0U; i < ORACLE_ACTIVE_SIZE; ++i)
        p[i] = 0U;
}

static void inspect_bank(uint32_t bank,
                         struct dposlab_oracle_bank_stats *out)
{
    volatile const uint8_t *p = (volatile const uint8_t *)(uintptr_t)
        (ORACLE_RAW_BASE + bank * ORACLE_BANK_STRIDE);
    uint32_t seen[8];
    uint32_t i;

    for (i = 0U; i < 8U; ++i)
        seen[i] = 0U;

    out->nonzero = 0U;
    out->first_nonzero = 0xFFFFFFFFU;
    out->last_nonzero = 0xFFFFFFFFU;
    out->minimum = 0xFFU;
    out->maximum = 0U;
    out->unique = 0U;
    out->hash = 2166136261U;

    for (i = 0U; i < ORACLE_ACTIVE_SIZE; ++i) {
        uint32_t value = p[i];
        uint32_t word = value >> 5;
        uint32_t bit = 1U << (value & 31U);

        if (value != 0U) {
            if (out->nonzero == 0U)
                out->first_nonzero = i;
            out->last_nonzero = i;
            ++out->nonzero;
        }
        if (value < out->minimum)
            out->minimum = value;
        if (value > out->maximum)
            out->maximum = value;
        if ((seen[word] & bit) == 0U) {
            seen[word] |= bit;
            ++out->unique;
        }
        out->hash ^= value;
        out->hash *= 16777619U;
    }

    out->data0 = load_u32(p + 0U);
    out->data1 = load_u32(p + 4U);
}

void dposlab_stock_init_custom_acq_probe(void)
{
    struct dposlab_stock_init_custom_acq_result *out =
        (struct dposlab_stock_init_custom_acq_result *)(uintptr_t)
        &dposlab_stock_init_custom_acq_result;
    struct acq_capture_result capture;
    struct fpga_protocol_ready_result ready;
    uint32_t stock_period;
    uint32_t stock_offset;
    uint32_t stock_origin;
    uint32_t stock_position;
    uint32_t bank;

    out->marker = ORACLE_RESULT_MARKER;
    out->sysconfig_called = 0U;
    out->status_before = 0U;
    out->status_after = 0U;
    out->polls = 0U;
    out->ready = 0U;
    out->pointer_valid = 0U;
    out->live_pointer = 0U;
    out->runtime_290 = 0U;
    out->runtime_63c = 0U;
    out->capture_position = 0U;
    out->raw_base = ORACLE_RAW_BASE;

    for (bank = 0U; bank < DPOSLAB_ORACLE_BANK_COUNT; ++bank) {
        out->bank[bank].done = 0U;
        clear_active_bank(bank);
    }
    if (cache_dcache_enabled())
        cache_invalidate_range(
            ORACLE_RAW_BASE,
            DPOSLAB_ORACLE_BANK_COUNT * ORACLE_BANK_STRIDE);
    cache_dsb();

    /* Final stock operation before the STOCK/CUSTOM boundary. */
    STOCK_SYS_CONFIG_EXEC();
    out->sysconfig_called = 1U;

    /*
     * VARIANT A STOCK SIDE:
     *   selector block 0 and the complete ACQ_RESTART wire sequence are stock.
     * READY is then observed by the custom protocol poller without issuing a
     * second restart. This exposes raw selector-0x0A first/final values and
     * the exact poll count, unlike stock SYS_INFO+0x38 which is only a bool.
     */
    STOCK_VARIANT_A_SELECT_BLOCK(0U);
    STOCK_VARIANT_A_PREPARE();
    fpga_protocol_acq_wait_ready(VARIANT_A_READY_POLLS, &ready);

    out->status_before = ready.first_status;
    out->status_after = ready.final_status;
    out->polls = ready.polls;
    out->ready = ready.ready ? 1U : 0U;
    if (out->ready == 0U)
        return;

    /*
     * VARIANT B STOCK SIDE AFTER READY:
     * Preserve stock pointer-update, runtime-math, and window-bound side
     * effects before crossing into custom stream and CSI capture.
     */
    STOCK_VARIANT_B_UPDATE_POSITION();
    STOCK_VARIANT_B_RUNTIME_MATH();
    STOCK_VARIANT_B_WINDOW_BOUNDS();

    stock_period = STOCK_STATE32(0x520U);
    stock_offset = STOCK_STATE32(0x63CU);
    stock_origin = STOCK_STATE16(0x6F2U);
    if (stock_period == 0U)
        return;

    stock_position =
        (stock_period + stock_offset - stock_origin) % stock_period;

    /*
     * VARIANT C STOCK SIDE:
     *   preserve stock STREAM_CONFIG side effects and ordering. This replaces
     * the custom acq_configure_stream() call completely.
     */
    acq_set_prepared_capture_position((uint16_t)stock_position);
    STOCK_VARIANT_C_STREAM_CONFIG();

    /* CUSTOM boundary: only the per-bank CSI capture loop remains custom. */

    out->pointer_valid = 0U;
    out->live_pointer = 0U;
    out->runtime_290 = STOCK_STATE32(0x290U);
    out->runtime_63c = stock_offset;
    out->capture_position = stock_position;
    for (bank = 0U; bank < DPOSLAB_ORACLE_BANK_COUNT; ++bank) {
        uintptr_t destination = (uintptr_t)
            (ORACLE_RAW_BASE + bank * ORACLE_BANK_STRIDE);

        out->bank[bank].done = acq_capture(
            destination,
            (uint16_t)stock_position,
            (uint8_t)bank,
            ORACLE_CAPTURE_TIMEOUT,
            &capture) ? 1U : 0U;
    }

    if (cache_dcache_enabled())
        cache_invalidate_range(
            ORACLE_RAW_BASE,
            DPOSLAB_ORACLE_BANK_COUNT * ORACLE_BANK_STRIDE);
    cache_dsb();

    for (bank = 0U; bank < DPOSLAB_ORACLE_BANK_COUNT; ++bank)
        inspect_bank(bank, &out->bank[bank]);
}

void dposlab_stock_init_custom_acq_monitor(void)
{
    const volatile struct dposlab_stock_init_custom_acq_result *r =
        &dposlab_stock_init_custom_acq_result;
    uint32_t bank;

    console_init(WHITE, BLACK, 2U);
    console_clear();
    console_putln("VAR C STOCK STREAM -> CUSTOM CSI");
    console_putln("--------------------------");
    console_puts("SYSCONFIG     ");
    console_putln(r->sysconfig_called ? "CALLED" : "NOT CALLED");
    console_puts("READY         ");
    console_putln(r->ready ? "YES" : "NO");
    console_puts("STATUS PRE/POST ");
    put_hex(r->status_before); console_puts("/");
    put_hex(r->status_after); console_putln("");
    console_puts("POLLS         "); put_hex(r->polls); console_putln("");
    console_puts("PTR V/LIVE    "); put_hex(r->pointer_valid); console_puts("/");
    put_hex(r->live_pointer); console_putln("");
    console_puts("290/63C       "); put_hex(r->runtime_290); console_puts("/");
    put_hex(r->runtime_63c); console_putln("");
    console_puts("POSITION      "); put_hex(r->capture_position); console_putln("");
    console_puts("RAW BASE      "); put_hex(r->raw_base); console_putln("");
    console_putln("");

    for (bank = 0U; bank < DPOSLAB_ORACLE_BANK_COUNT; ++bank) {
        console_puts("B");
        console_puts(bank == 0U ? "0" : bank == 1U ? "1" :
                     bank == 2U ? "2" : "3");
        console_puts(" DONE/NZ  ");
        put_hex(r->bank[bank].done); console_puts("/");
        put_hex(r->bank[bank].nonzero); console_putln("");
        console_puts("  FIRST/LAST ");
        put_hex(r->bank[bank].first_nonzero); console_puts("/");
        put_hex(r->bank[bank].last_nonzero); console_putln("");
        console_puts("  UNIQ/MINMAX ");
        put_hex(r->bank[bank].unique); console_puts("/");
        put_hex((r->bank[bank].minimum << 8) | r->bank[bank].maximum);
        console_putln("");
        console_puts("  HASH       "); put_hex(r->bank[bank].hash); console_putln("");
        console_puts("  DATA0/1    "); put_hex(r->bank[bank].data0); console_puts("/");
        put_hex(r->bank[bank].data1); console_putln("");
    }

    console_putln("POWER CYCLE TO EXIT");
    console_present();
}
