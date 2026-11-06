#include <stdint.h>

#include "cache.h"
#include "loader.h"
#include "usb_linux.h"

#define ARM_LDR_PC_PC_MINUS_4      0xE51FF004U
#define STOCK_EXE2_BASE            0x40000000U
#define STOCK_EXE2_ENTRY_WORD      0xE59FF018U
#define STOCK_SYSTEM_INIT_SITE     0x40034710U
#define STOCK_SYSTEM_INIT_ORIGINAL 0xEB003984U
#define STOCK_USB_LINUX_VENEER     0x40055B54U
#define USB_LINUX_LOAD_ADDRESS     0x42E70000U
#define USB_LINUX_WINDOW_SIZE      0x00010000U

extern const uint8_t usb_linux_blob_start[];
extern const uint8_t usb_linux_blob_end[];

static volatile uint32_t *word_at(uint32_t address)
{
    return (volatile uint32_t *)(uintptr_t)address;
}

static uint32_t blob_size(void)
{
    return (uint32_t)(usb_linux_blob_end - usb_linux_blob_start);
}

static void copy_blob(uint8_t *destination, const uint8_t *source,
                      uint32_t size)
{
    uint32_t i;

    for (i = 0U; i < size; ++i)
        destination[i] = source[i];
}

static int encode_arm_branch(uint32_t site, uint32_t target,
                             uint32_t *instruction)
{
    int32_t displacement;
    int32_t words;

    displacement = (int32_t)(target - (site + 8U));
    if ((displacement & 3) != 0 ||
        displacement < -0x02000000 ||
        displacement > 0x01FFFFFC)
        return 0;

    words = displacement >> 2;
    *instruction = 0xEA000000U |
                   ((uint32_t)words & 0x00FFFFFFU);
    return 1;
}

int usb_linux_install_patch(const loader_image_t *exe2_image)
{
    volatile uint32_t *site;
    volatile uint32_t *veneer;
    uint8_t *resident;
    uint32_t size;
    uint32_t branch;

    if (!exe2_image)
        return LOADER_ERR_ARGUMENT;

    if (exe2_image->load_address != STOCK_EXE2_BASE ||
        exe2_image->entry_address != STOCK_EXE2_BASE ||
        exe2_image->bytes_loaded != exe2_image->image_size)
        return LOADER_ERR_VERIFY;

    if (*word_at(STOCK_EXE2_BASE) != STOCK_EXE2_ENTRY_WORD)
        return LOADER_ERR_BAD_HEADER;

    size = blob_size();
    if (size == 0U || size > USB_LINUX_WINDOW_SIZE)
        return LOADER_ERR_BAD_SIZE;

    resident = (uint8_t *)(uintptr_t)USB_LINUX_LOAD_ADDRESS;
    copy_blob(resident, usb_linux_blob_start, size);
    cache_clean_range(USB_LINUX_LOAD_ADDRESS, size);

    site = word_at(STOCK_SYSTEM_INIT_SITE);
    veneer = word_at(STOCK_USB_LINUX_VENEER);

    if (*site != STOCK_SYSTEM_INIT_ORIGINAL)
        return LOADER_ERR_VERIFY;

    if (!encode_arm_branch(STOCK_SYSTEM_INIT_SITE,
                           STOCK_USB_LINUX_VENEER,
                           &branch))
        return LOADER_ERR_ADDRESS_RANGE;

    veneer[0] = ARM_LDR_PC_PC_MINUS_4;
    veneer[1] = USB_LINUX_LOAD_ADDRESS;
    *site = branch;

    cache_clean_range(STOCK_USB_LINUX_VENEER, 8U);
    cache_clean_range(STOCK_SYSTEM_INIT_SITE, 4U);
    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    if (*site != branch ||
        veneer[0] != ARM_LDR_PC_PC_MINUS_4 ||
        veneer[1] != USB_LINUX_LOAD_ADDRESS)
        return LOADER_ERR_VERIFY;

    return LOADER_OK;
}
