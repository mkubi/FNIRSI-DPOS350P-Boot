#include <stdint.h>

#include "spi0.h"

#define RESULT_ADDR 0x00009E00U

typedef struct {
    uint32_t magic;
    int32_t spi_init_result;
    int32_t wake_result;
    int32_t jedec_result;
    uint8_t wake_rx[4];
    uint8_t jedec_rx[4];
} nor_wake_result_t;

static volatile nor_wake_result_t * const result =
    (volatile nor_wake_result_t *)(uintptr_t)RESULT_ADDR;

static void settle_delay(void)
{
    volatile uint32_t i;

    for (i = 0U; i < 200000U; ++i)
        __asm__ volatile ("nop");
}

int nor_wake_main(void)
{
    uint8_t wake_tx[4] = { 0xABU, 0x00U, 0x00U, 0x00U };
    uint8_t wake_rx[4] = { 0U, 0U, 0U, 0U };

    uint8_t id_tx[4] = { 0x9FU, 0x00U, 0x00U, 0x00U };
    uint8_t id_rx[4] = { 0U, 0U, 0U, 0U };

    unsigned int i;

    result->magic = 0x454B4157U; /* WAKE */
    result->spi_init_result = -999;
    result->wake_result = -999;
    result->jedec_result = -999;

    result->spi_init_result = spi0_init();
    if (result->spi_init_result != 0)
        return -1;

    /*
     * Release from Deep Power-Down / Device ID transaction.
     * Four clocks after AB make the experiment explicit rather than
     * relying on the one-byte-only form.
     */
    result->wake_result = spi0_transfer(wake_tx, wake_rx, 4U);

    for (i = 0U; i < 4U; ++i)
        result->wake_rx[i] = wake_rx[i];

    if (result->wake_result != 0)
        return -2;

    settle_delay();

    /*
     * Verify the wake-up locally before returning to FEL.
     */
    result->jedec_result = spi0_transfer(id_tx, id_rx, 4U);

    for (i = 0U; i < 4U; ++i)
        result->jedec_rx[i] = id_rx[i];

    __asm__ volatile ("dsb sy" ::: "memory");

    return 0;
}
