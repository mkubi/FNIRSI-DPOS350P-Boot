#include <stdint.h>

#include "irq.h"
#include "sdmmc0.h"
#include "timer.h"

#define STOCK_USB_CONNECT_ENTRY      0x40069018U

#define STOCK_SECTOR_SIZE            0x4009548CU
#define STOCK_MEMORY_TYPE            0x40095490U
#define STOCK_CAPACITY_A1C           0x42E68A1CU
#define STOCK_CAPACITY_A20           0x42E68A20U
#define STOCK_CID_WORDS              0x42E68C00U
#define STOCK_CARD_TYPE              0x42E68CB8U
#define STOCK_CARD_RCA               0x42E68CBCU
#define STOCK_CAPACITY_CC0           0x42E68CC0U

#define EXPECTED_SECTORS             0x0003C800U
#define EXPECTED_HALF_CAPACITY       0x0001E400U
#define STOCK_CARD_TYPE_ORACLE       2U

typedef void (*stock_void_fn_t)(void);

static volatile uint32_t *word_at(uint32_t address)
{
    return (volatile uint32_t *)(uintptr_t)address;
}

static void stop(void) __attribute__((noreturn));
static void stop(void)
{
    for (;;)
        __asm__ volatile ("wfi");
}

static void publish_stock_sd_state(const sdmmc0_info_t *info)
{
    volatile uint32_t *cid =
        (volatile uint32_t *)(uintptr_t)STOCK_CID_WORDS;
    unsigned int i;

    *word_at(STOCK_SECTOR_SIZE)  = SDMMC0_SECTOR_SIZE;
    *word_at(STOCK_MEMORY_TYPE)  = 1U;
    *word_at(STOCK_CAPACITY_A1C) = info->capacity_sectors;
    *word_at(STOCK_CAPACITY_A20) = info->decode_x200;
    *word_at(STOCK_CARD_TYPE)    = STOCK_CARD_TYPE_ORACLE;
    *word_at(STOCK_CARD_RCA)     = info->rca;
    *word_at(STOCK_CAPACITY_CC0) = info->capacity_sectors;

    for (i = 0U; i < 4U; ++i)
        cid[i] = info->cid[i];
}

int main(void)
{
    sdmmc0_info_t info;
    int result;

    irq_controller_init();
    irq_enable_global();
    timer_init();

    result = sdmmc0_init(&info);
    if (result != 0 ||
        info.capacity_sectors != EXPECTED_SECTORS ||
        info.decode_x200 != EXPECTED_HALF_CAPACITY)
        stop();

    if (sdmmc0_prepare_stock_handoff() != 0)
        stop();

    publish_stock_sd_state(&info);

    ((stock_void_fn_t)(uintptr_t)STOCK_USB_CONNECT_ENTRY)();
    stop();
}
