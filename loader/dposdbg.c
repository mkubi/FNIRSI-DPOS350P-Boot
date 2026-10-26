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

static dposdbg_diag_t dposdbg_diag;

static void diag_clear(void)
{
    uint32_t *words = (uint32_t *)(void *)&dposdbg_diag;
    uint32_t i;

    for (i = 0U; i < sizeof(dposdbg_diag) / sizeof(*words); ++i)
        words[i] = 0U;
}

static void diag_begin(uint32_t phase)
{
    dposdbg_diag.phase = phase;
    dposdbg_diag.result = LOADER_OK;
}

static int diag_fail(int result)
{
    dposdbg_diag.result = result;
    return result;
}

static void diag_hook(const dposdbg_hook_t *hook, uint32_t index)
{
    dposdbg_diag.hook_index = index;
    dposdbg_diag.patch_type = hook->patch_type;
    dposdbg_diag.site = hook->site;
    dposdbg_diag.expected_word = hook->expected_word;
    dposdbg_diag.veneer_address = hook->veneer_address;
    dposdbg_diag.target_address = hook->target_address;
}

const dposdbg_diag_t *dposdbg_last_diag(void)
{
    return &dposdbg_diag;
}

const char *dposdbg_diag_phase_name(uint32_t phase)
{
    switch (phase) {
    case DPOSDBG_DIAG_IDLE: return "IDLE";
    case DPOSDBG_DIAG_SD_INIT: return "SD INIT";
    case DPOSDBG_DIAG_FAT_MOUNT: return "FAT MOUNT";
    case DPOSDBG_DIAG_FILE_LOOKUP: return "FILE LOOKUP";
    case DPOSDBG_DIAG_IMAGE_LOAD: return "IMAGE LOAD";
    case DPOSDBG_DIAG_HEADER_VALIDATE: return "HEADER VALIDATE";
    case DPOSDBG_DIAG_MANIFEST_VALIDATE: return "MANIFEST VALIDATE";
    case DPOSDBG_DIAG_STOCK_VALIDATE: return "STOCK VALIDATE";
    case DPOSDBG_DIAG_PATCH_WRITE: return "PATCH WRITE";
    case DPOSDBG_DIAG_CACHE_MAINT: return "CACHE MAINT";
    case DPOSDBG_DIAG_READBACK: return "READBACK";
    case DPOSDBG_DIAG_SUCCESS: return "SUCCESS";
    default: return "UNKNOWN";
    }
}

const char *dposdbg_patch_type_name(uint32_t patch_type)
{
    if (patch_type == DPOSDBG_PATCH_ARM_B)
        return "B";
    if (patch_type == DPOSDBG_PATCH_ARM_BL)
        return "BL";
    return "NONE";
}

static uint32_t arm_branch(uint32_t site, uint32_t target, uint32_t link)
{
    int32_t displacement = (int32_t)(target - (site + 8U));
    uint32_t opcode = link != 0U ? 0xEB000000U : 0xEA000000U;
    return opcode | (((uint32_t)(displacement >> 2)) & 0x00FFFFFFU);
}

static int branch_reachable(uint32_t site, uint32_t target)
{
    int32_t displacement;
    if (((site | target) & 3U) != 0U)
        return 0;
    displacement = (int32_t)(target - (site + 8U));
    return displacement >= -0x02000000 && displacement <= 0x01FFFFFC;
}

static int range_inside(uint32_t offset, uint32_t count,
                        uint32_t item_size, uint32_t limit)
{
    uint32_t bytes;
    uint32_t end;
    if (count != 0U && item_size > 0xFFFFFFFFU / count)
        return 0;
    bytes = count * item_size;
    end = offset + bytes;
    return end >= offset && offset >= sizeof(dposdbg_header_t) && end <= limit;
}

static int header_valid(const dposdbg_header_t *header, uint32_t file_size)
{
    uint32_t image_end;
    if (header->magic != DPOSDBG_MAGIC ||
        header->abi_version != DPOSDBG_ABI_VERSION)
        return 0;
    if (header->image_size != file_size ||
        header->image_size < sizeof(*header) ||
        header->image_size > DPOSDBG_MAX_SIZE)
        return 0;
    image_end = DPOSDBG_LOAD_ADDRESS + header->image_size;
    if (image_end < DPOSDBG_LOAD_ADDRESS || image_end > DPOSDBG_RESIDENT_LIMIT)
        return 0;
    if (header->trace_address < DPOSDBG_LOAD_ADDRESS ||
        header->trace_address >= image_end)
        return 0;
    if (header->hook_count == 0U || header->hook_count > DPOSDBG_MAX_HOOKS)
        return 0;
    if (!range_inside(header->hook_table_offset, header->hook_count,
                      sizeof(dposdbg_hook_t), header->image_size))
        return 0;
    return header->flags == 0U && header->reserved == 0U;
}

static int hook_valid(const dposdbg_hook_t *hook, uint32_t image_end)
{
    if (hook->site < ORIGINAL_APP_BASE || hook->site >= ORIGINAL_APP_END ||
        (hook->site & 3U) != 0U)
        return 0;
    if (hook->veneer_address < DPOSDBG_VENEER_MIN ||
        hook->veneer_address + 8U < hook->veneer_address ||
        hook->veneer_address + 8U > DPOSDBG_VENEER_LIMIT ||
        (hook->veneer_address & 3U) != 0U)
        return 0;
    if (hook->target_address < DPOSDBG_LOAD_ADDRESS ||
        hook->target_address >= image_end || (hook->target_address & 3U) != 0U)
        return 0;
    if (hook->patch_type != DPOSDBG_PATCH_ARM_B &&
        hook->patch_type != DPOSDBG_PATCH_ARM_BL)
        return 0;
    return hook->flags == 0U && branch_reachable(hook->site, hook->veneer_address);
}

int dposdbg_load_and_patch(void)
{
    sdmmc0_info_t sd_info;
    fat_volume_t volume;
    fat_file_t file;
    dposdbg_header_t *header;
    const dposdbg_hook_t *hooks;
    uint8_t *destination;
    uint32_t sectors, sector, image_end, i;
    int result;

    diag_clear();
    diag_begin(DPOSDBG_DIAG_SD_INIT);
    result = sdmmc0_init(&sd_info);
    if (result)
        return diag_fail(LOADER_ERR_READ);

    diag_begin(DPOSDBG_DIAG_FAT_MOUNT);
    result = fat_mount_first(&volume);
    if (result)
        return diag_fail(LOADER_ERR_READ);

    diag_begin(DPOSDBG_DIAG_FILE_LOOKUP);
    result = fat_find_root(&volume, dposdbg_name, &file);
    if (result)
        return diag_fail(LOADER_ERR_NOT_FOUND);
    dposdbg_diag.file_size = file.size;
    if (file.size < sizeof(dposdbg_header_t) || file.size > DPOSDBG_MAX_SIZE)
        return diag_fail(LOADER_ERR_BAD_SIZE);

    diag_begin(DPOSDBG_DIAG_IMAGE_LOAD);
    destination = (uint8_t *)(uintptr_t)DPOSDBG_LOAD_ADDRESS;
    sectors = (file.size + FAT_SECTOR_SIZE - 1U) / FAT_SECTOR_SIZE;
    for (sector = 0U; sector < sectors; ++sector) {
        dposdbg_diag.sector_index = sector;
        result = fat_read_file_sector(&volume, &file, sector,
                                      destination + sector * FAT_SECTOR_SIZE);
        if (result)
            return diag_fail(LOADER_ERR_READ);
    }

    diag_begin(DPOSDBG_DIAG_HEADER_VALIDATE);
    header = (dposdbg_header_t *)(uintptr_t)DPOSDBG_LOAD_ADDRESS;
    dposdbg_diag.abi_version = header->abi_version;
    dposdbg_diag.image_size = header->image_size;
    dposdbg_diag.hook_count = header->hook_count;
    dposdbg_diag.hook_table_offset = header->hook_table_offset;
    if (!header_valid(header, file.size))
        return diag_fail(LOADER_ERR_VERIFY);

    image_end = DPOSDBG_LOAD_ADDRESS + header->image_size;
    hooks = (const dposdbg_hook_t *)(uintptr_t)
        (DPOSDBG_LOAD_ADDRESS + header->hook_table_offset);

    diag_begin(DPOSDBG_DIAG_MANIFEST_VALIDATE);
    for (i = 0U; i < header->hook_count; ++i) {
        volatile const uint32_t *site;
        uint32_t j;
        diag_hook(&hooks[i], i);
        if (!hook_valid(&hooks[i], image_end))
            return diag_fail(LOADER_ERR_VERIFY);
        for (j = 0U; j < i; ++j) {
            if (hooks[j].site == hooks[i].site)
                return diag_fail(LOADER_ERR_VERIFY);
            if (hooks[j].veneer_address == hooks[i].veneer_address &&
                hooks[j].target_address != hooks[i].target_address)
                return diag_fail(LOADER_ERR_VERIFY);
        }
        site = (volatile const uint32_t *)(uintptr_t)hooks[i].site;
        dposdbg_diag.actual_word = *site;
    }

    diag_begin(DPOSDBG_DIAG_STOCK_VALIDATE);
    for (i = 0U; i < header->hook_count; ++i) {
        volatile const uint32_t *site =
            (volatile const uint32_t *)(uintptr_t)hooks[i].site;
        diag_hook(&hooks[i], i);
        dposdbg_diag.actual_word = *site;
        if (dposdbg_diag.actual_word != hooks[i].expected_word)
            return diag_fail(LOADER_ERR_VERIFY);
    }

    diag_begin(DPOSDBG_DIAG_PATCH_WRITE);
    for (i = 0U; i < header->hook_count; ++i) {
        volatile uint32_t *site =
            (volatile uint32_t *)(uintptr_t)hooks[i].site;
        volatile uint32_t *veneer =
            (volatile uint32_t *)(uintptr_t)hooks[i].veneer_address;
        uint32_t link = hooks[i].patch_type == DPOSDBG_PATCH_ARM_BL;
        diag_hook(&hooks[i], i);
        dposdbg_diag.generated_word =
            arm_branch(hooks[i].site, hooks[i].veneer_address, link);
        veneer[0] = 0xE51FF004U;
        veneer[1] = hooks[i].target_address;
        *site = dposdbg_diag.generated_word;
        cache_clean_range(hooks[i].veneer_address, 8U);
        cache_clean_range(hooks[i].site, sizeof(*site));
    }

    diag_begin(DPOSDBG_DIAG_CACHE_MAINT);
    cache_clean_range(DPOSDBG_LOAD_ADDRESS, header->image_size);
    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    diag_begin(DPOSDBG_DIAG_READBACK);
    for (i = 0U; i < header->hook_count; ++i) {
        volatile const uint32_t *site =
            (volatile const uint32_t *)(uintptr_t)hooks[i].site;
        volatile const uint32_t *veneer =
            (volatile const uint32_t *)(uintptr_t)hooks[i].veneer_address;
        uint32_t link = hooks[i].patch_type == DPOSDBG_PATCH_ARM_BL;
        diag_hook(&hooks[i], i);
        dposdbg_diag.generated_word =
            arm_branch(hooks[i].site, hooks[i].veneer_address, link);
        dposdbg_diag.actual_word = *site;
        dposdbg_diag.veneer_word = veneer[0];
        dposdbg_diag.veneer_target = veneer[1];
        if (dposdbg_diag.veneer_word != 0xE51FF004U ||
            dposdbg_diag.veneer_target != hooks[i].target_address ||
            dposdbg_diag.actual_word != dposdbg_diag.generated_word)
            return diag_fail(LOADER_ERR_VERIFY);
    }

    diag_begin(DPOSDBG_DIAG_SUCCESS);
    dposdbg_diag.hook_index = header->hook_count;
    dposdbg_diag.result = LOADER_OK;
    return LOADER_OK;
}
