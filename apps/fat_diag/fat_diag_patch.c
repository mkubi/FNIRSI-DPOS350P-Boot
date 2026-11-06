#include <stdint.h>
#include "fat_diag_patch.h"
#include "cache.h"
#include "timer.h"
#include "irq.h"
#include "sdmmc0.h"

#define ARM_LDR_PC_PC_MINUS_4 0xE51FF004U
#define STOCK_SYSTEM_INIT_SITE 0x40034710U
#define STOCK_SYSTEM_INIT_ORIGINAL 0xEB003984U
#define FAT_DIAG_VENEER 0x40055B54U
#define STOCK_USB_CONNECT_ENTRY 0x40069018U

#define STOCK_SECTOR_SIZE      0x4009548CU
#define STOCK_MEMORY_TYPE      0x40095490U
#define STOCK_CAPACITY_A1C     0x42E68A1CU
#define STOCK_CAPACITY_A20     0x42E68A20U
#define STOCK_CID_WORDS        0x42E68C00U
#define STOCK_CARD_TYPE        0x42E68CB8U
#define STOCK_CARD_RCA         0x42E68CBCU
#define STOCK_CAPACITY_CC0     0x42E68CC0U

#define EXPECTED_SECTORS       0x0003C800U
#define EXPECTED_HALF_CAPACITY 0x0001E400U
#define STOCK_CARD_TYPE_ORACLE 2U

typedef void (*stock_void_fn_t)(void);

static volatile uint32_t *word_at(uint32_t a)
{
    return (volatile uint32_t *)(uintptr_t)a;
}

static int encode_arm_branch(uint32_t s, uint32_t t, uint32_t link,
                             uint32_t *op)
{
    int32_t d = (int32_t)(t - (s + 8U));
    int32_t w;
    uint32_t b;

    if ((d & 3) != 0 || d < -0x02000000 || d > 0x01FFFFFC)
        return 0;

    w = d >> 2;
    b = link ? 0xEB000000U : 0xEA000000U;
    *op = b | ((uint32_t)w & 0x00FFFFFFU);
    return 1;
}

static void s06usb_publish_stock_state(const sdmmc0_info_t *info)
{
    volatile uint32_t *cid =
        (volatile uint32_t *)(uintptr_t)STOCK_CID_WORDS;
    unsigned i;

    /* HW-CONFIRMED S0.4A stock storage-state fingerprint. */
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

void fat_diag_s06usb_hook(void) __attribute__((noreturn));
void fat_diag_s06usb_hook(void)
{
    sdmmc0_info_t info;
    int rc;

    irq_controller_init();
    irq_enable_global();
    timer_init();

    rc = sdmmc0_init(&info);
    if (rc != 0 ||
        info.capacity_sectors != EXPECTED_SECTORS ||
        info.decode_x200 != EXPECTED_HALF_CAPACITY)
        for (;;)
            __asm__ volatile("wfi");

    if (sdmmc0_prepare_stock_handoff() != 0)
    for (;;)
        __asm__ volatile("wfi");

    s06usb_publish_stock_state(&info);

    /* S0.6 USB discriminator: custom SD init + stock USB only. */
    ((stock_void_fn_t)(uintptr_t)STOCK_USB_CONNECT_ENTRY)();

    for (;;)
        __asm__ volatile("wfi");
}

int fat_diag_install_patch(void)
{
    volatile uint32_t *site = word_at(STOCK_SYSTEM_INIT_SITE);
    volatile uint32_t *v = word_at(FAT_DIAG_VENEER);
    uint32_t branch;
    uint32_t hook = (uint32_t)(uintptr_t)fat_diag_s06usb_hook;

    if (*site != STOCK_SYSTEM_INIT_ORIGINAL)
        return 0;
    if (!encode_arm_branch(STOCK_SYSTEM_INIT_SITE, FAT_DIAG_VENEER,
                           0U, &branch))
        return 0;

    v[0] = ARM_LDR_PC_PC_MINUS_4;
    v[1] = hook;
    *site = branch;

    cache_clean_range(FAT_DIAG_VENEER, 8U);
    cache_clean_range(STOCK_SYSTEM_INIT_SITE, 4U);
    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    return *site == branch &&
           v[0] == ARM_LDR_PC_PC_MINUS_4 &&
           v[1] == hook;
}
