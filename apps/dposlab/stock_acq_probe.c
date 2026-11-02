#include <stdint.h>

#include "stock_acq_probe.h"
#include "../../arch/r11/cache.h"
#include "../../ui/console.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U

#define STOCK_APP_STATE_BASE       0x40544E68U
#define STOCK_RAW_BASE             0x42DF76D4U
#define STOCK_RAW_BANK_STRIDE      0x00002134U
#define STOCK_RAW_ACTIVE_SIZE      0x00001800U

#define STOCK_SELECT_BLOCK         ((void (*)(uint32_t))(uintptr_t)0x4006338CU)
#define STOCK_ACQ_PREPARE          ((void (*)(void))(uintptr_t)0x4003FA10U)
#define STOCK_ACQ_WAIT_READY       ((void (*)(void))(uintptr_t)0x4003FCC0U)
#define STOCK_ACQ_UPDATE_POSITION  ((void (*)(void))(uintptr_t)0x4003FAE4U)
#define STOCK_ACQ_RUNTIME_MATH     ((void (*)(void))(uintptr_t)0x4003FB90U)
#define STOCK_ACQ_WINDOW_BOUNDS    ((void (*)(void))(uintptr_t)0x40040030U)
#define STOCK_STREAM_CONFIG        ((void (*)(void))(uintptr_t)0x400405E0U)
#define STOCK_CAPTURE_BANKS_0_3    ((void (*)(void))(uintptr_t)0x400406FCU)

#define STATE8(offset)  (*(volatile uint8_t  *)(uintptr_t)(STOCK_APP_STATE_BASE + (offset)))
#define STATE16(offset) (*(volatile uint16_t *)(uintptr_t)(STOCK_APP_STATE_BASE + (offset)))
#define STATE32(offset) (*(volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + (offset)))

volatile struct dposlab_stock_acq_result dposlab_stock_acq_result
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

static void inspect_bank(uint32_t bank,
                         struct dposlab_stock_bank_stats *out)
{
    volatile const uint8_t *p = (volatile const uint8_t *)(uintptr_t)
        (STOCK_RAW_BASE + bank * STOCK_RAW_BANK_STRIDE);
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

    for (i = 0U; i < STOCK_RAW_ACTIVE_SIZE; ++i) {
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

void dposlab_stock_acq_probe(void)
{
    struct dposlab_stock_acq_result *out =
        (struct dposlab_stock_acq_result *)&dposlab_stock_acq_result;
    uint32_t block = 0U;
    uint32_t computed_position;
    uint32_t i;

    out->status_before = 0U;
    out->status_after = 0U;
    out->ready = 0U;
    out->period = 0U;
    out->offset = 0U;
    out->origin = 0U;
    out->position = 0U;
    out->raw_base = STOCK_RAW_BASE;

    /* Match the first stock runtime iteration: block selector 0. */
    STOCK_SELECT_BLOCK(block);
    STOCK_ACQ_PREPARE();

    out->status_before = STATE8(0x038U);
    STOCK_ACQ_WAIT_READY();
    out->status_after = STATE8(0x038U);
    out->ready = out->status_after != 0U ? 1U : 0U;

    if (out->ready != 0U) {
        STOCK_ACQ_UPDATE_POSITION();
        STOCK_ACQ_RUNTIME_MATH();
        STOCK_ACQ_WINDOW_BOUNDS();

        out->period = STATE32(0x520U);
        out->offset = STATE32(0x63CU);
        out->origin = STATE16(0x6F2U);
        if (out->period != 0U) {
            computed_position =
                (out->period + out->offset - out->origin) % out->period;
            out->position = computed_position;
        }

STOCK_STREAM_CONFIG();

/*
 * ACQ1.1:
 * Stock 0x40040638 waits on APP_STATE +0x70E, which is published
 * by the stock CSI interrupt/completion path.
 */
__asm__ volatile (
    "dsb sy\n"
    "isb sy\n"
    "cpsie i\n"
    ::: "memory"
);

STOCK_CAPTURE_BANKS_0_3();

__asm__ volatile (
    "cpsid i\n"
    "dsb sy\n"
    "isb sy\n"
    ::: "memory"
);


        if (cache_dcache_enabled())
            cache_invalidate_range(STOCK_RAW_BASE,
                4U * STOCK_RAW_BANK_STRIDE);
        cache_dsb();
    }

    for (i = 0U; i < 4U; ++i)
        inspect_bank(i, &out->bank[i]);
}

void dposlab_stock_acq_monitor(void)
{
    const volatile struct dposlab_stock_acq_result *r =
        &dposlab_stock_acq_result;
    uint32_t i;

    console_init(WHITE, BLACK, 2U);
    console_clear();
    console_putln("DPOSLAB STOCK ACQ1");
    console_putln("--------------------------");
    console_puts("READY         "); console_putln(r->ready ? "YES" : "NO");
    console_puts("STATUS PRE/POST "); put_hex(r->status_before); console_puts("/"); put_hex(r->status_after); console_putln("");
    console_puts("PERIOD/OFFSET "); put_hex(r->period); console_puts("/"); put_hex(r->offset); console_putln("");
    console_puts("ORIGIN/POS    "); put_hex(r->origin); console_puts("/"); put_hex(r->position); console_putln("");
    console_puts("RAW BASE      "); put_hex(r->raw_base); console_putln("");
    console_putln("");

    for (i = 0U; i < 4U; ++i) {
        console_puts("B");
        console_puts(i == 0U ? "0" : i == 1U ? "1" : i == 2U ? "2" : "3");
        console_puts(" NZ/FIRST   "); put_hex(r->bank[i].nonzero); console_puts("/"); put_hex(r->bank[i].first_nonzero); console_putln("");
        console_puts("   LAST/UNIQ "); put_hex(r->bank[i].last_nonzero); console_puts("/"); put_hex(r->bank[i].unique); console_putln("");
        console_puts("   MIN/MAX   "); put_hex(r->bank[i].minimum); console_puts("/"); put_hex(r->bank[i].maximum); console_putln("");
        console_puts("   HASH      "); put_hex(r->bank[i].hash); console_putln("");
        console_puts("   DATA0/1   "); put_hex(r->bank[i].data0); console_puts("/"); put_hex(r->bank[i].data1); console_putln("");
    }

    console_putln("POWER CYCLE TO EXIT");
    console_present();
}
