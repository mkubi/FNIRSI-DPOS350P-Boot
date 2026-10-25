#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"
#include "sdmmc0.h"
#include "fat.h"
#include "spi_nor_ro.h"
#include "spi_nor_stage2_rw.h"

#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define BLACK  0x0000U

#define STAGE2_MAGIC           0x32475453U
#define STAGE2_VERSION         1U
#define STAGE2_HEADER_SIZE     32U
#define STAGE2_SLOT_SIZE       0x00010000U
#define STAGE2_NOR_OFFSET      0x001DB000U
#define STAGE2_LOAD_ADDRESS    0x41000000U
#define STAGE2_MAX_PAYLOAD     (STAGE2_SLOT_SIZE - STAGE2_HEADER_SIZE)

static const char stage2_name[11] = {
    'S','T','A','G','E','2',' ',' ','B','I','N'
};
static const char confirm_name[11] = {
    'S','T','G','2','O','K',' ',' ','B','I','N'
};
static const uint8_t confirm_token[16] = {
    'S','T','G','2','W','R','I','T','E','-','O','K','-','v','1','\n'
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t payload_size;
    uint32_t load_address;
    uint32_t entry_address;
    uint32_t payload_crc32;
    uint32_t flags;
} stage2_header_t;

typedef struct {
    stage2_header_t header;
    uint32_t file_size;
    uint32_t calculated_crc32;
    uint32_t slot_calculated_crc32;
    uint32_t slot_header_crc32;
    uint32_t differences;
    uint32_t first_difference;
    int exact_match;
    int input_valid;
    int slot_valid;
} updater_result_t;

static uint8_t sector_buffer[FAT_SECTOR_SIZE] __attribute__((aligned(4)));
static uint8_t nor_buffer[FAT_SECTOR_SIZE] __attribute__((aligned(4)));
static uint8_t old_slot[STAGE2_SLOT_SIZE] __attribute__((aligned(32)));
static uint8_t new_slot[STAGE2_SLOT_SIZE] __attribute__((aligned(32)));

static char hex_digit(uint32_t value)
{
    value &= 0xFU;
    return value < 10U ? (char)('0' + value) : (char)('A' + value - 10U);
}

static void put_hex32(uint32_t value)
{
    char out[11];
    unsigned i;
    out[0] = '0';
    out[1] = 'x';
    for (i = 0U; i < 8U; ++i)
        out[2U + i] = hex_digit(value >> (28U - 4U * i));
    out[10] = '\0';
    console_puts(out);
}

static void line_hex(const char *label, uint32_t value)
{
    console_puts(label);
    put_hex32(value);
    console_putln("");
}

static void status(const char *label, int ok)
{
    console_status(label, ok ? "PASS" : "FAIL", ok ? GREEN : RED);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t i;
    unsigned bit;
    for (i = 0U; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return crc;
}

static int header_valid(const stage2_header_t *h, uint32_t file_size)
{
    uint32_t payload_end;
    if (file_size != STAGE2_SLOT_SIZE)
        return 0;
    if (h->magic != STAGE2_MAGIC || h->version != STAGE2_VERSION)
        return 0;
    if (h->header_size != STAGE2_HEADER_SIZE)
        return 0;
    if (h->payload_size == 0U || h->payload_size > STAGE2_MAX_PAYLOAD)
        return 0;
    if (h->load_address != STAGE2_LOAD_ADDRESS || h->flags != 0U)
        return 0;
    payload_end = h->load_address + h->payload_size;
    if (payload_end < h->load_address)
        return 0;
    if (h->entry_address < h->load_address || h->entry_address >= payload_end)
        return 0;
    return 1;
}

static int analyze_input(fat_volume_t *volume, fat_file_t *file,
                         updater_result_t *r)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t payload_remaining;
    uint32_t file_sector;
    uint32_t skip;
    uint32_t take;
    int result;

    r->file_size = file->size;
    result = fat_read_file_sector(volume, file, 0U, sector_buffer);
    if (result)
        return result;

    r->header.magic = read_le32(&sector_buffer[0]);
    r->header.version = read_le32(&sector_buffer[4]);
    r->header.header_size = read_le32(&sector_buffer[8]);
    r->header.payload_size = read_le32(&sector_buffer[12]);
    r->header.load_address = read_le32(&sector_buffer[16]);
    r->header.entry_address = read_le32(&sector_buffer[20]);
    r->header.payload_crc32 = read_le32(&sector_buffer[24]);
    r->header.flags = read_le32(&sector_buffer[28]);

    if (!header_valid(&r->header, r->file_size))
        return 0;

    payload_remaining = r->header.payload_size;
    file_sector = 0U;
    skip = STAGE2_HEADER_SIZE;
    while (payload_remaining != 0U) {
        if (file_sector != 0U) {
            result = fat_read_file_sector(volume, file, file_sector,
                                          sector_buffer);
            if (result)
                return result;
            skip = 0U;
        }
        take = FAT_SECTOR_SIZE - skip;
        if (take > payload_remaining)
            take = payload_remaining;
        crc = crc32_update(crc, &sector_buffer[skip], take);
        payload_remaining -= take;
        ++file_sector;
    }

    r->calculated_crc32 = crc ^ 0xFFFFFFFFU;
    r->input_valid = r->calculated_crc32 == r->header.payload_crc32;
    return 0;
}

static int analyze_current_slot(fat_volume_t *volume, fat_file_t *file,
                                updater_result_t *r)
{
    stage2_header_t h;
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t payload_remaining;
    uint32_t sector;
    uint32_t skip;
    uint32_t take;
    uint32_t offset;
    uint32_t i;
    int result;

    result = spi_nor_ro_init();
    if (result)
        return result;
    result = spi_nor_read_chunked(STAGE2_NOR_OFFSET, nor_buffer,
                                  FAT_SECTOR_SIZE);
    if (result)
        return result;

    h.magic = read_le32(&nor_buffer[0]);
    h.version = read_le32(&nor_buffer[4]);
    h.header_size = read_le32(&nor_buffer[8]);
    h.payload_size = read_le32(&nor_buffer[12]);
    h.load_address = read_le32(&nor_buffer[16]);
    h.entry_address = read_le32(&nor_buffer[20]);
    h.payload_crc32 = read_le32(&nor_buffer[24]);
    h.flags = read_le32(&nor_buffer[28]);
    r->slot_header_crc32 = h.payload_crc32;

    if (!header_valid(&h, STAGE2_SLOT_SIZE))
        return 0;

    payload_remaining = h.payload_size;
    sector = 0U;
    skip = STAGE2_HEADER_SIZE;
    while (payload_remaining != 0U) {
        if (sector != 0U) {
            result = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                          sector * FAT_SECTOR_SIZE,
                                          nor_buffer, FAT_SECTOR_SIZE);
            if (result)
                return result;
            skip = 0U;
        }
        take = FAT_SECTOR_SIZE - skip;
        if (take > payload_remaining)
            take = payload_remaining;
        crc = crc32_update(crc, &nor_buffer[skip], take);
        payload_remaining -= take;
        ++sector;
    }
    r->slot_calculated_crc32 = crc ^ 0xFFFFFFFFU;
    r->slot_valid = r->slot_calculated_crc32 == h.payload_crc32;

    r->differences = 0U;
    r->first_difference = 0xFFFFFFFFU;
    for (sector = 0U; sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE; ++sector) {
        result = fat_read_file_sector(volume, file, sector, sector_buffer);
        if (result)
            return result;
        result = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                      sector * FAT_SECTOR_SIZE,
                                      nor_buffer, FAT_SECTOR_SIZE);
        if (result)
            return result;
        for (i = 0U; i < FAT_SECTOR_SIZE; ++i) {
            if (sector_buffer[i] != nor_buffer[i]) {
                offset = sector * FAT_SECTOR_SIZE + i;
                if (r->first_difference == 0xFFFFFFFFU)
                    r->first_difference = offset;
                r->differences += 1U;
            }
        }
    }
    r->exact_match = r->differences == 0U;
    return 0;
}


static int load_file_image(fat_volume_t *volume, fat_file_t *file,
                           uint8_t *destination)
{
    uint32_t sector;
    int rc;
    for (sector = 0U; sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE; ++sector) {
        rc = fat_read_file_sector(volume, file, sector,
                                  destination + sector * FAT_SECTOR_SIZE);
        if (rc)
            return rc;
    }
    return 0;
}

static int load_nor_image(uint8_t *destination)
{
    return spi_nor_read_chunked(STAGE2_NOR_OFFSET, destination,
                                STAGE2_SLOT_SIZE);
}

static int confirm_valid(fat_volume_t *volume, fat_file_t *file)
{
    uint32_t i;
    int rc;
    if (file->size != sizeof(confirm_token))
        return 0;
    rc = fat_read_file_sector(volume, file, 0U, sector_buffer);
    if (rc)
        return 0;
    for (i = 0U; i < sizeof(confirm_token); ++i)
        if (sector_buffer[i] != confirm_token[i])
            return 0;
    return 1;
}

static int verify_nor_image(const uint8_t *expected,
                            uint32_t *first_difference)
{
    uint32_t sector;
    uint32_t i;
    uint32_t offset;
    int rc;

    *first_difference = 0xFFFFFFFFU;
    for (sector = 0U; sector < STAGE2_SLOT_SIZE / FAT_SECTOR_SIZE; ++sector) {
        rc = spi_nor_read_chunked(STAGE2_NOR_OFFSET +
                                  sector * FAT_SECTOR_SIZE,
                                  nor_buffer, FAT_SECTOR_SIZE);
        if (rc)
            return rc;
        for (i = 0U; i < FAT_SECTOR_SIZE; ++i) {
            offset = sector * FAT_SECTOR_SIZE + i;
            if (nor_buffer[i] != expected[offset]) {
                *first_difference = offset;
                return 1;
            }
        }
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

static int program_stage2_slot(const uint8_t *image)
{
    return spi_nor_stage2_program(STAGE2_NOR_OFFSET, image,
                                  STAGE2_SLOT_SIZE);
}

static int restore_previous_slot(uint32_t *first_difference)
{
    int rc = erase_stage2_slot();
    if (rc)
        return rc;
    rc = program_stage2_slot(old_slot);
    if (rc)
        return rc;
    rc = verify_nor_image(old_slot, first_difference);
    return rc == 0 ? 0 : (rc > 0 ? -40 : rc);
}

int main(void)
{
    sdmmc0_info_t sd_info;
    fat_volume_t volume;
    fat_file_t file;
    fat_file_t confirm_file;
    static updater_result_t result;
    uint32_t verify_first_difference;
    int rc;

    display_init();
    console_init(WHITE, BLACK, 1U);
    timer_init();
    console_clear();
    console_set_color(YELLOW);
    console_putln("STAGE 2 UPDATER - U1.1 SECTOR-VERIFY WRITER");
    console_set_color(WHITE);
    console_putln("DANGER: WRITES ONLY WITH VALID /STG2OK.BIN TOKEN");
    console_putln("");

    rc = sdmmc0_init(&sd_info);
    status("SD-NAND INIT       ", rc == 0);
    if (rc != 0)
        goto done;

    rc = fat_mount_first(&volume);
    status("FAT MOUNT          ", rc == 0);
    if (rc != 0)
        goto done;

    rc = fat_find_root(&volume, stage2_name, &file);
    status("STAGE2.BIN FOUND   ", rc == 0);
    if (rc != 0)
        goto done;

    rc = analyze_input(&volume, &file, &result);
    status("INPUT READ         ", rc == 0);
    if (rc != 0)
        goto done;

    line_hex("FILE SIZE          ", result.file_size);
    line_hex("MAGIC              ", result.header.magic);
    line_hex("VERSION            ", result.header.version);
    line_hex("HEADER SIZE        ", result.header.header_size);
    line_hex("PAYLOAD SIZE       ", result.header.payload_size);
    line_hex("LOAD ADDRESS       ", result.header.load_address);
    line_hex("ENTRY ADDRESS      ", result.header.entry_address);
    line_hex("HEADER CRC32       ", result.header.payload_crc32);
    line_hex("CALCULATED CRC32   ", result.calculated_crc32);
    status("INPUT VALID        ", result.input_valid);
    if (!result.input_valid)
        goto done;

    rc = analyze_current_slot(&volume, &file, &result);
    status("NOR READ           ", rc == 0);
    if (rc != 0)
        goto done;
    line_hex("CURRENT HDR CRC32  ", result.slot_header_crc32);
    line_hex("CURRENT CALC CRC32 ", result.slot_calculated_crc32);
    status("CURRENT SLOT VALID ", result.slot_valid);
    line_hex("BYTE DIFFERENCES   ", result.differences);
    line_hex("FIRST DIFFERENCE   ", result.first_difference);
    status("EXACT SLOT MATCH   ", result.exact_match);

    rc = load_file_image(&volume, &file, new_slot);
    status("NEW IMAGE IN RAM   ", rc == 0);
    if (rc != 0)
        goto done;
    rc = load_nor_image(old_slot);
    status("OLD SLOT BACKUP    ", rc == 0);
    if (rc != 0)
        goto done;

    rc = fat_find_root(&volume, confirm_name, &confirm_file);
    status("STG2OK.BIN FOUND   ", rc == 0);
    if (rc != 0)
        goto armed_off;
    if (!confirm_valid(&volume, &confirm_file)) {
        status("CONFIRM TOKEN     ", 0);
        goto armed_off;
    }
    status("CONFIRM TOKEN     ", 1);

    console_putln("");
    console_set_color(RED);
    console_putln("WRITE ARMED - DO NOT REMOVE POWER");
    console_present();
    timer_delay_ms(2000U);

    rc = spi_nor_stage2_rw_init();
    status("NOR WRITE INIT     ", rc == 0);
    console_present();
    if (rc != 0)
        goto done;

    rc = erase_stage2_slot();
    status("ERASE 16 SECTORS  ", rc == 0);
    console_present();
    if (rc != 0)
        goto restore;

    rc = program_stage2_slot(new_slot);
    status("PROGRAM 64 KIB    ", rc == 0);
    console_present();
    if (rc != 0)
        goto restore;

    rc = verify_nor_image(new_slot, &verify_first_difference);
    status("READ-BACK         ", rc >= 0);
    if (rc < 0)
        goto restore;
    if (rc != 0) {
        status("BYTE VERIFY       ", 0);
        line_hex("VERIFY FIRST DIFF ", verify_first_difference);
        console_present();
        goto restore;
    }
    status("BYTE VERIFY       ", 1);
    console_set_color(GREEN);
    console_putln("UPDATE SUCCESS - POWER CYCLE NOW");
    goto done;

restore:
    console_set_color(RED);
    console_putln("UPDATE FAILED - RESTORING OLD SLOT");
    console_present();
    rc = restore_previous_slot(&verify_first_difference);
    status("OLD SLOT RESTORE   ", rc == 0);
    if (rc == -40)
        line_hex("RESTORE FIRST DIFF", verify_first_difference);
    if (rc == 0)
        console_putln("RESTORE SUCCESS - POWER CYCLE");
    else
        console_putln("RESTORE FAILED - USE MANUAL FEL RECOVERY");
    goto done;

armed_off:
    console_putln("");
    console_set_color(YELLOW);
    console_putln("WRITE NOT ARMED - NO NOR CHANGES MADE");
    console_putln("CREATE /STG2OK.BIN WITH EXACT 16-BYTE TOKEN");
    console_putln("STG2WRITE-OK-v1\n");

done:
    console_present();
    for (;;)
        __asm__ volatile ("wfi");
}
