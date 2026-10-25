#include <stddef.h>
#include <stdint.h>
#include "dposdbg.h"
#include "cache.h"
#include "loader.h"
#include "fat.h"
#include "sdmmc0.h"


#define ORIGINAL_APP_BASE 0x40000000U
#define ORIGINAL_APP_END  0x40100000U

static const char dposdbg_name[11] = {
    'D','P','O','S','D','B','G',' ','B','I','N'
};

static uint32_t arm_branch(uint32_t site, uint32_t target)
{
    int32_t displacement;

    displacement = (int32_t)(target - (site + 8U));
    return 0xEA000000U | (((uint32_t)(displacement >> 2)) & 0x00FFFFFFU);
}

static uint32_t arm_branch_link(uint32_t site, uint32_t target)
{
    int32_t displacement;

    displacement = (int32_t)(target - (site + 8U));

    return 0xEB000000U |
           (((uint32_t)(displacement >> 2)) & 0x00FFFFFFU);
}

typedef struct {
    uint32_t site;
    uint32_t expected;
    uint32_t veneer;
} dposdbg_call_patch_t;

static const dposdbg_call_patch_t primitive_patches[] = {
    { 0x40049E1CU, 0xEBFFFD16U, DPOSDBG_SELECTOR_VENEER },
    { 0x40049E20U, 0xEBFFFCE4U, DPOSDBG_READ_VENEER },
    { 0x40049F6CU, 0xEBFFFCC2U, DPOSDBG_SELECTOR_VENEER },
    { 0x40049F88U, 0xEBFFFCBBU, DPOSDBG_SELECTOR_VENEER },
    { 0x40049F90U, 0xEBFFFC9FU, DPOSDBG_WRITE_VENEER },
    { 0x40049F9CU, 0xEBFFFC9CU, DPOSDBG_WRITE_VENEER },
    { 0x40049FA8U, 0xEBFFFC99U, DPOSDBG_WRITE_VENEER },
    { 0x40049FC4U, 0xEBFFFCACU, DPOSDBG_SELECTOR_VENEER },
    { 0x40049FCCU, 0xEBFFFC90U, DPOSDBG_WRITE_VENEER },
    { 0x40049FE8U, 0xEBFFFCA3U, DPOSDBG_SELECTOR_VENEER },
    { 0x40049FF0U, 0xEBFFFC87U, DPOSDBG_WRITE_VENEER },
    { 0x40049FF8U, 0xEBFFFC9FU, DPOSDBG_SELECTOR_VENEER },
    { 0x40049FFCU, 0xEBFFFC6DU, DPOSDBG_READ_VENEER },
    { 0x4004A00CU, 0xEBFFFC9AU, DPOSDBG_SELECTOR_VENEER },
    { 0x4004A02CU, 0xEBFFFC92U, DPOSDBG_SELECTOR_VENEER },
    { 0x4004A030U, 0xEBFFFC60U, DPOSDBG_READ_VENEER },
    { 0x4004A04CU, 0xEBFFFC8AU, DPOSDBG_SELECTOR_VENEER },
    { 0x4004A050U, 0xEBFFFC58U, DPOSDBG_READ_VENEER },
    { 0x4004A070U, 0xEBFFFC81U, DPOSDBG_SELECTOR_VENEER },
    { 0x4004A088U, 0xEBFFFC7BU, DPOSDBG_SELECTOR_VENEER },
    { 0x4004A08CU, 0xEBFFFC49U, DPOSDBG_READ_VENEER },
    { 0x4004A094U, 0xEBFFFC47U, DPOSDBG_READ_VENEER },
    { 0x4004A09CU, 0xEBFFFC45U, DPOSDBG_READ_VENEER },
    { 0x4004A0A4U, 0xEBFFFC43U, DPOSDBG_READ_VENEER },
    { 0x4004A148U, 0xEBFFFC4BU, DPOSDBG_SELECTOR_VENEER },
};

typedef struct {
    uint32_t site;
    uint32_t expected;
    uint32_t veneer;
} dposdbg_tail_patch_t;

static const dposdbg_tail_patch_t tail_patches[] = {
    { 0x40049F78U, 0xEAFFFCA5U, DPOSDBG_WRITE_VENEER },
    { 0x40049FB4U, 0xEAFFFC96U, DPOSDBG_WRITE_VENEER },
    { 0x40049FD8U, 0xEAFFFC8DU, DPOSDBG_WRITE_VENEER },
    { 0x4004A01CU, 0xEAFFFC7CU, DPOSDBG_WRITE_VENEER },
    { 0x4004A07CU, 0xEAFFFC4DU, DPOSDBG_READ_VENEER },
};

static int branch_reachable(uint32_t site, uint32_t target)
{
    int32_t displacement;

    if ((target & 3U) != 0U)
        return 0;

    displacement = (int32_t)(target - (site + 8U));
    return displacement >= -0x02000000 && displacement <= 0x01FFFFFC;
}

static int header_valid(const dposdbg_header_t *header, uint32_t file_size)
{
    uint32_t image_end;

    if (header->magic != DPOSDBG_MAGIC)
        return 0;
    if (header->abi_version != DPOSDBG_ABI_VERSION)
        return 0;
    if (header->image_size != file_size)
        return 0;
    if (header->image_size < sizeof(*header) ||
        header->image_size > DPOSDBG_MAX_SIZE)
        return 0;

    image_end = DPOSDBG_LOAD_ADDRESS + header->image_size;
    if (image_end < DPOSDBG_LOAD_ADDRESS)
        return 0;
    if (image_end > DPOSDBG_RESIDENT_LIMIT)
        return 0;
    if (header->screenshot_hook < DPOSDBG_LOAD_ADDRESS ||
        header->screenshot_hook >= image_end)
        return 0;
    if (header->selector_wrapper < DPOSDBG_LOAD_ADDRESS ||
        header->selector_wrapper >= image_end ||
        header->write_wrapper < DPOSDBG_LOAD_ADDRESS ||
        header->write_wrapper >= image_end ||
        header->read_wrapper < DPOSDBG_LOAD_ADDRESS ||
        header->read_wrapper >= image_end)
        return 0;
    if (!branch_reachable(DPOSDBG_SCREENSHOT_HOOK,
                          DPOSDBG_VENEER_ADDRESS))
        return 0;
    return 1;
}

int dposdbg_load_and_patch(void)
{
    sdmmc0_info_t sd_info;
    fat_volume_t volume;
    fat_file_t file;
    dposdbg_header_t *header;
    uint8_t *destination;
    uint32_t sectors;
    uint32_t sector;
    int result;

    result = sdmmc0_init(&sd_info);
    if (result)
        return LOADER_ERR_READ;

    result = fat_mount_first(&volume);
    if (result)
        return LOADER_ERR_READ;

    result = fat_find_root(&volume, dposdbg_name, &file);
    if (result)
        return LOADER_ERR_NOT_FOUND;

    if (file.size < sizeof(dposdbg_header_t) ||
        file.size > DPOSDBG_MAX_SIZE)
        return LOADER_ERR_BAD_SIZE;

    destination = (uint8_t *)(uintptr_t)DPOSDBG_LOAD_ADDRESS;
    sectors = (file.size + FAT_SECTOR_SIZE - 1U) / FAT_SECTOR_SIZE;

    for (sector = 0U; sector < sectors; ++sector) {
        result = fat_read_file_sector(&volume, &file, sector,
                                      destination + sector * FAT_SECTOR_SIZE);
        if (result)
            return LOADER_ERR_READ;
    }

    header = (dposdbg_header_t *)(uintptr_t)DPOSDBG_LOAD_ADDRESS;
    if (!header_valid(header, file.size))
        return LOADER_ERR_VERIFY;

    /*
     * Phase 0.5A screenshot-specific pure-tail hook.
     * Patch 0x4005325C and preserve its original continuation at 0x40053388.
     */

    {
        volatile uint32_t *patch_word;
        volatile uint32_t *veneer;
        volatile uint32_t *selector_veneer;
        volatile uint32_t *write_veneer;
        volatile uint32_t *read_veneer;
        uint32_t branch_word;
        uint32_t i;

        patch_word =
            (volatile uint32_t *)(uintptr_t)DPOSDBG_SCREENSHOT_HOOK;
        veneer = (volatile uint32_t *)(uintptr_t)DPOSDBG_VENEER_ADDRESS;
        selector_veneer =
            (volatile uint32_t *)(uintptr_t)DPOSDBG_SELECTOR_VENEER;
        write_veneer =
            (volatile uint32_t *)(uintptr_t)DPOSDBG_WRITE_VENEER;
        read_veneer =
            (volatile uint32_t *)(uintptr_t)DPOSDBG_READ_VENEER;

        /* Validate every stock BL before changing any SMEC call site. */
        for (i = 0U;
             i < sizeof(primitive_patches) / sizeof(primitive_patches[0]);
             ++i) {
            volatile uint32_t *call_word =
                (volatile uint32_t *)(uintptr_t)primitive_patches[i].site;

            if (*call_word != primitive_patches[i].expected)
                return LOADER_ERR_VERIFY;
        }

        for (i = 0U;
             i < sizeof(tail_patches) / sizeof(tail_patches[0]); ++i) {
            volatile uint32_t *tail_word =
                (volatile uint32_t *)(uintptr_t)tail_patches[i].site;
            if (*tail_word != tail_patches[i].expected)
                return LOADER_ERR_VERIFY;
        }

        /* ARM veneers: LDR pc, [pc, #-4], followed by absolute targets. */
        veneer[0] = 0xE51FF004U;
        veneer[1] = header->screenshot_hook;
        selector_veneer[0] = 0xE51FF004U;
        selector_veneer[1] = header->selector_wrapper;
        write_veneer[0] = 0xE51FF004U;
        write_veneer[1] = header->write_wrapper;
        read_veneer[0] = 0xE51FF004U;
        read_veneer[1] = header->read_wrapper;

        branch_word = arm_branch(
            DPOSDBG_SCREENSHOT_HOOK,
            DPOSDBG_VENEER_ADDRESS);
        *patch_word = branch_word;

        for (i = 0U;
             i < sizeof(primitive_patches) / sizeof(primitive_patches[0]);
             ++i) {
            volatile uint32_t *call_word =
                (volatile uint32_t *)(uintptr_t)primitive_patches[i].site;

            *call_word = arm_branch_link(primitive_patches[i].site,
                                         primitive_patches[i].veneer);
            cache_clean_range(primitive_patches[i].site,
                              sizeof(*call_word));
        }

        /*
         * Preserve tail-call semantics: stock B sites remain B sites. The
         * existing wrappers return through the LR inherited from the caller.
         */
        for (i = 0U;
             i < sizeof(tail_patches) / sizeof(tail_patches[0]); ++i) {
            volatile uint32_t *tail_word =
                (volatile uint32_t *)(uintptr_t)tail_patches[i].site;
            *tail_word = arm_branch(tail_patches[i].site,
                                    tail_patches[i].veneer);
            cache_clean_range(tail_patches[i].site, sizeof(*tail_word));
        }

        cache_clean_range(DPOSDBG_LOAD_ADDRESS, header->image_size);
        cache_clean_range(DPOSDBG_VENEER_ADDRESS,
                          8U * sizeof(*veneer));
        cache_clean_range(DPOSDBG_SCREENSHOT_HOOK, sizeof(*patch_word));
        cache_invalidate_icache_all();
        cache_invalidate_branch_predictor();
        cache_dsb();
        cache_isb();

        if (veneer[0] != 0xE51FF004U ||
            veneer[1] != header->screenshot_hook ||
            selector_veneer[1] != header->selector_wrapper ||
            write_veneer[1] != header->write_wrapper ||
            read_veneer[1] != header->read_wrapper ||
            *patch_word != branch_word)
            return LOADER_ERR_VERIFY;
    }    return LOADER_OK;
}
