#include <stdint.h>

#include "stage2_update.h"
#include "sdmmc0.h"
#include "fat.h"
#include "spi_nor_ro.h"
#include "spi_nor_stage2_rw.h"

#define STAGE2_MAGIC           0x32475453U
#define STAGE2_VERSION         1U
#define STAGE2_HEADER_SIZE     32U
#define STAGE2_SLOT_SIZE       0x00010000U
#define STAGE2_NOR_OFFSET      0x001DB000U
#define STAGE2_LOAD_ADDRESS    0x41000000U
#define STAGE2_MAX_PAYLOAD     (STAGE2_SLOT_SIZE - STAGE2_HEADER_SIZE)

/*
 * HW-confirmed resident RAM window. The updater uses it only while Stage 2 is
 * active; DPOSLAB/USB resident payloads are not running at the same time.
 */
#define STAGE2_BACKUP_ADDRESS  0x42E70000U

static const char stage2_name[11] = {
    'S','T','A','G','E','2',' ',' ','B','I','N'
};
static const char confirm_name[11] = {
    'S','T','G','2','O','K',' ',' ','B','I','N'
};
static const uint8_t confirm_token[16] = {
    'S','T','G','2','W','R','I','T','E','-','O','K','-','v','1','\n'
};

static uint8_t file_buffer[FAT_SECTOR_SIZE] __attribute__((aligned(4)));
static uint8_t nor_buffer[FAT_SECTOR_SIZE] __attribute__((aligned(4)));

static uint8_t *old_slot(void)
{
    return (uint8_t *)(uintptr_t)STAGE2_BACKUP_ADDRESS;
}

static void clear_words(void *object, uint32_t size)
{
    uint32_t *word = (uint32_t *)object;
    uint32_t count = size / sizeof(uint32_t);
    uint32_t i;

    for (i = 0U; i < count; ++i)
        word[i] = 0U;
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data,
                             uint32_t length)
{
    uint32_t i;
    unsigned bit;

    for (i = 0U; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1) ^
                  (0xEDB88320U & (0U - (crc & 1U)));
    }
    return crc;
}

static int header_valid_values(uint32_t file_size, uint32_t magic,
                               uint32_t version, uint32_t header_size,
                               uint32_t payload_size, uint32_t load_address,
                               uint32_t entry_address)
{
    uint32_t payload_end;

    if (file_size != STAGE2_SLOT_SIZE)
        return 0;
    if (magic != STAGE2_MAGIC || version != STAGE2_VERSION)
        return 0;
    if (header_size != STAGE2_HEADER_SIZE)
        return 0;
    if (payload_size == 0U || payload_size > STAGE2_MAX_PAYLOAD)
        return 0;
    if (load_address != STAGE2_LOAD_ADDRESS)
        return 0;
    payload_end = load_address + payload_size;
    if (payload_end < load_address)
        return 0;
    if (entry_address < load_address || entry_address >= payload_end)
        return 0;
    return 1;
}

static int analyze_input(fat_volume_t *volume, fat_file_t *file,
                         stage2_update_info_t *info)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t remaining;
    uint32_t sector = 0U;
    uint32_t skip = STAGE2_HEADER_SIZE;
    uint32_t take;
    int rc;

    info->file_size = file->size;
    rc = fat_read_file_sector(volume, file, 0U, file_buffer);
    if (rc)
        return rc;

    info->magic = read_le32(&file_buffer[0]);
    info->version = read_le32(&file_buffer[4]);
    info->header_size = read_le32(&file_buffer[8]);
    info->payload_size = read_le32(&file_buffer[12]);
    info->load_address = read_le32(&file_buffer[16]);
    info->entry_address = read_le32(&file_buffer[20]);
    info->expected_crc32 = read_le32(&file_buffer[24]);
    info->header_valid = header_valid_values(
        info->file_size, info->magic, info->version, info->header_size,
        info->payload_size, info->load_address, info->entry_address);
    if (!info->header_valid)
        return 0;

    remaining = info->payload_size;
    while (remaining != 0U) {
        if (sector != 0U) {
            rc = fat_read_file_sector(volume, file, sector, file_buffer);
            if (rc)
                return rc;
            skip = 0U;
        }
        take = FAT_SECTOR_SIZE - skip;
        if (take > remaining)
            take = remaining;
        crc = crc32_update(crc, &file_buffer[skip], take);
        remaining -= take;
        ++sector;
    }

    info->calculated_crc32 = crc ^ 0xFFFFFFFFU;
    info->input_valid = info->calculated_crc32 == info->expected_crc32;
    return 0;
}

static int analyze_slot(fat_volume_t *volume, fat_file_t *file,
                        stage2_update_info_t *info)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t payload_size;
    uint32_t remaining;
    uint32_t sector;
    uint32_t skip = STAGE2_HEADER_SIZE;
    uint32_t take;
    uint32_t i;
    uint32_t offset;
    int rc;

    rc = spi_nor_ro_init();
    if (rc)
        return rc;
    rc = spi_nor_read_chunked(STAGE2_NOR_OFFSET, nor_buffer,
                              FAT_SECTOR_SIZE);
    if (rc)
        return rc;

    payload_size = read_le32(&nor_buffer[12]);
    info->installed_expected_crc32 = read_le32(&nor_buffer[24]);
    if (!header_valid_values(
            STAGE2_SLOT_SIZE,
            read_le32(&nor_buffer[0]), read_le32(&nor_buffer[4]),
            read_le32(&nor_buffer[8]), payload_size,
            read_le32(&nor_buffer[16]), read_le32(&nor_buffer[20])))
        return 0;

    remaining = payload_size;
    sector = 0U;
    while (remaining != 0U) {
        if (sector != 0U) {
            rc = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                      sector * FAT_SECTOR_SIZE,
                                      nor_buffer, FAT_SECTOR_SIZE);
            if (rc)
                return rc;
            skip = 0U;
        }
        take = FAT_SECTOR_SIZE - skip;
        if (take > remaining)
            take = remaining;
        crc = crc32_update(crc, &nor_buffer[skip], take);
        remaining -= take;
        ++sector;
    }

    info->installed_calculated_crc32 = crc ^ 0xFFFFFFFFU;
    info->slot_valid = info->installed_calculated_crc32 ==
                       info->installed_expected_crc32;

    info->differences = 0U;
    info->first_difference = 0xFFFFFFFFU;
    for (sector = 0U;
         sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE;
         ++sector) {
        rc = fat_read_file_sector(volume, file, sector, file_buffer);
        if (rc)
            return rc;
        rc = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                  sector * FAT_SECTOR_SIZE,
                                  nor_buffer, FAT_SECTOR_SIZE);
        if (rc)
            return rc;
        for (i = 0U; i < FAT_SECTOR_SIZE; ++i) {
            if (file_buffer[i] != nor_buffer[i]) {
                offset = sector * FAT_SECTOR_SIZE + i;
                if (info->first_difference == 0xFFFFFFFFU)
                    info->first_difference = offset;
                ++info->differences;
            }
        }
    }

    info->exact_match = info->differences == 0U;
    return 0;
}


int stage2_tail_scan(stage2_tail_sector_t *sectors, unsigned capacity)
{
    unsigned sector;
    uint32_t chunk;
    uint32_t i;
    uint32_t address;
    int rc;

    if (!sectors || capacity < STAGE2_TAIL_SECTOR_COUNT)
        return -1;

    rc = spi_nor_ro_init();
    if (rc)
        return rc;

    for (sector = 0U; sector < STAGE2_TAIL_SECTOR_COUNT; ++sector) {
        stage2_tail_sector_t *out = &sectors[sector];
        address = STAGE2_TAIL_FIRST_ADDRESS + sector * 0x1000U;
        out->address = address;
        out->non_ff_count = 0U;
        out->first_non_ff = 0xFFFFFFFFU;
        out->first_word = 0xFFFFFFFFU;
        out->read_result = 0;

        for (chunk = 0U; chunk < 0x1000U; chunk += FAT_SECTOR_SIZE) {
            rc = spi_nor_read_chunked(address + chunk,
                                      nor_buffer, FAT_SECTOR_SIZE);
            if (rc) {
                out->read_result = rc;
                return rc;
            }
            if (chunk == 0U)
                out->first_word = read_le32(nor_buffer);
            for (i = 0U; i < FAT_SECTOR_SIZE; ++i) {
                if (nor_buffer[i] != 0xFFU) {
                    if (out->first_non_ff == 0xFFFFFFFFU)
                        out->first_non_ff = chunk + i;
                    ++out->non_ff_count;
                }
            }
        }
    }
    return 0;
}

int stage2_tail_verify_erased(uint32_t address,
                              uint32_t *non_ff_count,
                              uint32_t *first_non_ff)
{
    uint32_t chunk;
    uint32_t i;
    int rc;

    if (!non_ff_count || !first_non_ff)
        return -1;

    *non_ff_count = 0U;
    *first_non_ff = 0xFFFFFFFFU;

    rc = spi_nor_ro_init();
    if (rc)
        return rc;

    for (chunk = 0U; chunk < 0x1000U; chunk += FAT_SECTOR_SIZE) {
        rc = spi_nor_read_chunked(address + chunk,
                                  nor_buffer, FAT_SECTOR_SIZE);
        if (rc)
            return rc;
        for (i = 0U; i < FAT_SECTOR_SIZE; ++i) {
            if (nor_buffer[i] != 0xFFU) {
                if (*first_non_ff == 0xFFFFFFFFU)
                    *first_non_ff = chunk + i;
                ++*non_ff_count;
            }
        }
    }

    return 0;
}

int stage2_update_analyze(stage2_update_info_t *info)
{
    sdmmc0_info_t sd_info;
    fat_volume_t volume;
    fat_file_t file;
    int rc;

    if (!info)
        return -1;

    clear_words(info, (uint32_t)sizeof(*info));
    info->first_difference = 0xFFFFFFFFU;

    rc = sdmmc0_init(&sd_info);
    info->sd_result = rc;
    if (rc)
        return rc;
    rc = fat_mount_first(&volume);
    info->fat_result = rc;
    if (rc)
        return rc;
    rc = fat_find_root(&volume, stage2_name, &file);
    info->file_result = rc;
    info->file_found = rc == 0;
    if (rc)
        return rc;
    rc = analyze_input(&volume, &file, info);
    info->input_read_result = rc;
    if (rc || !info->input_valid)
        return rc;
    rc = analyze_slot(&volume, &file, info);
    info->nor_result = rc;
    return rc;
}

static int confirm_valid(fat_volume_t *volume, fat_file_t *file)
{
    uint32_t i;
    int rc;

    if (file->size != sizeof(confirm_token))
        return 0;
    rc = fat_read_file_sector(volume, file, 0U, file_buffer);
    if (rc)
        return 0;
    for (i = 0U; i < sizeof(confirm_token); ++i)
        if (file_buffer[i] != confirm_token[i])
            return 0;
    return 1;
}

static int backup_current_slot(void)
{
    uint32_t sector;
    int rc;

    for (sector = 0U;
         sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE;
         ++sector) {
        rc = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                  sector * FAT_SECTOR_SIZE,
                                  old_slot() + sector * FAT_SECTOR_SIZE,
                                  FAT_SECTOR_SIZE);
        if (rc)
            return rc;
    }
    return 0;
}

static int erase_stage2_slot(void)
{
    uint32_t address;
    int rc;

    for (address = STAGE2_NOR_OFFSET;
         address < STAGE2_NOR_OFFSET + STAGE2_SLOT_SIZE;
         address += SPI_NOR_STAGE2_SECTOR_SIZE) {
        rc = spi_nor_stage2_erase_sector(address);
        if (rc)
            return rc;
    }
    return 0;
}

static int program_file_slot(fat_volume_t *volume, fat_file_t *file)
{
    uint32_t sector;
    int rc;

    for (sector = 0U;
         sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE;
         ++sector) {
        rc = fat_read_file_sector(volume, file, sector, file_buffer);
        if (rc)
            return rc;
        rc = spi_nor_stage2_program(STAGE2_NOR_OFFSET +
                                    sector * FAT_SECTOR_SIZE,
                                    file_buffer, FAT_SECTOR_SIZE);
        if (rc)
            return rc;
    }
    return 0;
}

static int verify_file_slot(fat_volume_t *volume, fat_file_t *file,
                            uint32_t *first_difference)
{
    uint32_t sector;
    uint32_t i;
    int rc;

    *first_difference = 0xFFFFFFFFU;
    for (sector = 0U;
         sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE;
         ++sector) {
        rc = fat_read_file_sector(volume, file, sector, file_buffer);
        if (rc)
            return rc;
        rc = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                  sector * FAT_SECTOR_SIZE,
                                  nor_buffer, FAT_SECTOR_SIZE);
        if (rc)
            return rc;
        for (i = 0U; i < FAT_SECTOR_SIZE; ++i) {
            if (file_buffer[i] != nor_buffer[i]) {
                *first_difference = sector * FAT_SECTOR_SIZE + i;
                return 1;
            }
        }
    }
    return 0;
}

static int verify_memory_slot(const uint8_t *expected,
                              uint32_t *first_difference)
{
    uint32_t sector;
    uint32_t i;
    int rc;

    *first_difference = 0xFFFFFFFFU;
    for (sector = 0U;
         sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE;
         ++sector) {
        rc = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                  sector * FAT_SECTOR_SIZE,
                                  nor_buffer, FAT_SECTOR_SIZE);
        if (rc)
            return rc;
        for (i = 0U; i < FAT_SECTOR_SIZE; ++i) {
            if (expected[sector * FAT_SECTOR_SIZE + i] != nor_buffer[i]) {
                *first_difference = sector * FAT_SECTOR_SIZE + i;
                return 1;
            }
        }
    }
    return 0;
}

static int restore_old_slot(uint32_t *first_difference)
{
    int rc;

    rc = erase_stage2_slot();
    if (rc)
        return rc;
    rc = spi_nor_stage2_program(STAGE2_NOR_OFFSET, old_slot(),
                                STAGE2_SLOT_SIZE);
    if (rc)
        return rc;
    rc = verify_memory_slot(old_slot(), first_difference);
    return rc == 0 ? 0 : (rc > 0 ? -40 : rc);
}

stage2_write_status_t stage2_update_write(stage2_write_result_t *result)
{
    sdmmc0_info_t sd_info;
    fat_volume_t volume;
    fat_file_t file;
    fat_file_t confirm_file;
    stage2_update_info_t info;
    int rc;

    if (!result)
        return STAGE2_WRITE_INPUT_INVALID;

    clear_words(result, (uint32_t)sizeof(*result));
    result->first_difference = 0xFFFFFFFFU;
    result->backup_result = STAGE2_STEP_NOT_RUN;
    result->write_init_result = STAGE2_STEP_NOT_RUN;
    result->erase_result = STAGE2_STEP_NOT_RUN;
    result->program_result = STAGE2_STEP_NOT_RUN;
    result->verify_result = STAGE2_STEP_NOT_RUN;
    result->restore_result = STAGE2_STEP_NOT_RUN;

    rc = sdmmc0_init(&sd_info);
    if (rc)
        goto invalid;
    rc = fat_mount_first(&volume);
    if (rc)
        goto invalid;
    rc = fat_find_root(&volume, stage2_name, &file);
    if (rc)
        goto invalid;
    clear_words(&info, (uint32_t)sizeof(info));
    rc = analyze_input(&volume, &file, &info);
    if (rc || !info.input_valid)
        goto invalid;

    rc = fat_find_root(&volume, confirm_name, &confirm_file);
    result->confirm_found = rc == 0;
    if (rc || !confirm_valid(&volume, &confirm_file)) {
        result->status = STAGE2_WRITE_NOT_ARMED;
        return result->status;
    }
    result->confirm_valid = 1;

    rc = spi_nor_ro_init();
    if (rc)
        goto backup_failed;
    rc = backup_current_slot();
    result->backup_result = rc;
    if (rc)
        goto backup_failed;

    rc = spi_nor_stage2_rw_init();
    result->write_init_result = rc;
    if (rc) {
        result->status = STAGE2_WRITE_INIT_FAILED;
        return result->status;
    }

    rc = erase_stage2_slot();
    result->erase_result = rc;
    if (rc) {
        result->status = STAGE2_WRITE_ERASE_FAILED;
        goto restore;
    }

    rc = program_file_slot(&volume, &file);
    result->program_result = rc;
    if (rc) {
        result->status = STAGE2_WRITE_PROGRAM_FAILED;
        goto restore;
    }

    rc = verify_file_slot(&volume, &file, &result->first_difference);
    result->verify_result = rc;
    if (rc) {
        result->status = STAGE2_WRITE_VERIFY_FAILED;
        goto restore;
    }

    result->status = STAGE2_WRITE_OK;
    return result->status;

restore:
    rc = restore_old_slot(&result->first_difference);
    result->restore_result = rc;
    result->status = rc == 0
                   ? STAGE2_WRITE_RESTORED
                   : STAGE2_WRITE_RESTORE_FAILED;
    return result->status;

backup_failed:
    result->status = STAGE2_WRITE_BACKUP_FAILED;
    return result->status;

invalid:
    result->status = STAGE2_WRITE_INPUT_INVALID;
    return result->status;
}
