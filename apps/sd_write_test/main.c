#include <stdint.h>

#include "display.h"
#include "console.h"
#include "mmu.h"
#include "timer.h"
#include "sdmmc0.h"
#include "fat.h"

#define WHITE 0xFFFFU
#define BLACK 0x0000U
#define GREEN 0x07E0U
#define RED   0xF800U

#define WRTEST_SIZE 512U

static uint32_t original_words[WRTEST_SIZE / 4U] __attribute__((aligned(64)));
static uint32_t pattern_words[WRTEST_SIZE / 4U] __attribute__((aligned(64)));
static uint32_t verify_words[WRTEST_SIZE / 4U] __attribute__((aligned(64)));

static void put_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[11];
    uint32_t i;

    text[0] = '0';
    text[1] = 'x';
    for (i = 0U; i < 8U; ++i)
        text[2U + i] = hex[(value >> (28U - i * 4U)) & 0x0FU];
    text[10] = '\0';
    console_puts(text);
}

static void put_result(const char *label, int pass)
{
    console_puts(label);
    console_set_color(pass ? GREEN : RED);
    console_putln(pass ? "YES" : "NO");
    console_set_color(WHITE);
}

static int same_sector(const uint32_t *a, const uint32_t *b)
{
    uint32_t i;
    for (i = 0U; i < WRTEST_SIZE / 4U; ++i) {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

static void make_pattern(void)
{
    uint32_t i;
    for (i = 0U; i < WRTEST_SIZE / 4U; ++i)
        pattern_words[i] = 0x57525430U ^ (i * 0x01010101U);
}

int main(void)
{
    static const char wrtest_name[11] = {
        'W','R','T','E','S','T',' ',' ','B','I','N'
    };
    sdmmc0_info_t sd;
    fat_volume_t volume;
    fat_file_t file;
    int sd_result;
    int mount_result = -999;
    int find_result = -999;
    int size_ok = 0;
    int read_original_result = -999;
    int write_result = -999;
    int readback_result = -999;
    int verify_ok = 0;
    int restore_result = -999;
    int restore_read_result = -999;
    int restore_ok = 0;

    mmu_enable_identity_dram_cache(0);
    display_init();
    console_init(WHITE, BLACK, 2U);
    timer_init();

    console_clear();
    console_putln("SD SINGLE-SECTOR WRITE TEST");
    console_putln("WRTEST.BIN MUST BE 512 BYTES");
    console_putln("DO NOT REMOVE POWER");
    console_present();

    make_pattern();
    sd_result = sdmmc0_init(&sd);
    if (sd_result == 0)
        mount_result = fat_mount_first(&volume);
    if (mount_result == 0)
        find_result = fat_find_root(&volume, wrtest_name, &file);
    if (find_result == 0)
        size_ok = file.size == WRTEST_SIZE;

    if (size_ok) {
        read_original_result = sdmmc0_read_sector(
            file.first_sector_lba, (uint8_t *)original_words);
        if (read_original_result == 0)
            write_result = sdmmc0_write_sector(
                file.first_sector_lba, (const uint8_t *)pattern_words);
        if (write_result == 0)
            readback_result = sdmmc0_read_sector(
                file.first_sector_lba, (uint8_t *)verify_words);
        if (readback_result == 0)
            verify_ok = same_sector(pattern_words, verify_words);

        if (read_original_result == 0) {
            restore_result = sdmmc0_write_sector(
                file.first_sector_lba, (const uint8_t *)original_words);
            if (restore_result == 0)
                restore_read_result = sdmmc0_read_sector(
                    file.first_sector_lba, (uint8_t *)verify_words);
            if (restore_read_result == 0)
                restore_ok = same_sector(original_words, verify_words);
        }
    }

    console_clear();
    console_putln("SD WRITE/RESTORE RESULT");
    console_putln("-----------------------");
    put_result("SD INIT       ", sd_result == 0);
    put_result("FAT MOUNT     ", mount_result == 0);
    put_result("WRTEST FIND   ", find_result == 0);
    put_result("SIZE 512      ", size_ok);
    put_result("READ ORIGINAL ", read_original_result == 0);
    put_result("WRITE PATTERN ", write_result == 0);
    put_result("READBACK      ", readback_result == 0);
    put_result("VERIFY        ", verify_ok);
    put_result("RESTORE       ", restore_result == 0);
    put_result("RESTORE VERIFY", restore_ok);
    console_putln("");
    console_puts("SD ERR        "); put_hex32((uint32_t)sd_result); console_putln("");
    console_puts("MOUNT/FIND    "); put_hex32((uint32_t)mount_result);
    console_puts("/"); put_hex32((uint32_t)find_result); console_putln("");
    console_puts("WRITE/READ    "); put_hex32((uint32_t)write_result);
    console_puts("/"); put_hex32((uint32_t)readback_result); console_putln("");
    console_puts("RESTORE       "); put_hex32((uint32_t)restore_result);
    console_puts("/"); put_hex32((uint32_t)restore_read_result); console_putln("");
    console_present();

    for (;;)
        __asm__ volatile ("wfi");
}
