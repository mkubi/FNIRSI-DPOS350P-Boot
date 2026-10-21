#include <stdint.h>
#include "display.h"
#include "console.h"
#include "sdmmc0.h"
#include "fat.h"

#define BLACK  0x0000U
#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define CYAN   0x07FFU

static char hexn(uint8_t v)
{
    return v < 10U ? (char)('0' + v) : (char)('A' + v - 10U);
}

static void hex32(uint32_t v)
{
    unsigned i;
    char b[9];

    for (i = 0U; i < 8U; i++)
        b[i] = hexn((uint8_t)((v >> (28U - 4U * i)) & 15U));
    b[8] = 0;
    console_puts(b);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void value_line(const char *name, uint32_t value, uint16_t color)
{
    console_set_color(WHITE);
    console_puts(name);
    console_puts(" ");
    console_set_color(color);
    hex32(value);
    console_putln("");
}

static void bytes_line(const char *name, const uint8_t *data, unsigned count)
{
    unsigned i;

    console_set_color(WHITE);
    console_puts(name);
    console_puts(" ");
    console_set_color(CYAN);

    for (i = 0U; i < count; i++) {
        char b[3] = {hexn(data[i] >> 4), hexn(data[i] & 15U), 0};
        console_puts(b);
        if (i + 1U != count)
            console_puts(" ");
    }
    console_putln("");
}

int main(void)
{
    static uint32_t file_words[FAT_SECTOR_SIZE / 4U];
    uint8_t *file_sector = (uint8_t *)file_words;
    static const char hello_name[11] = {
        'H','E','L','L','O',' ',' ',' ','B','I','N'
    };
    sdmmc0_info_t sd;
    fat_volume_t volume;
    fat_file_t hello;
    int sd_result;
    int mount_result = -1;
    int find_result = -1;
    int read_result = -1;

    display_init();
    console_init(WHITE, BLACK, 1U);
    console_set_color(CYAN);
    console_putln("DPOS350P FAT16/FAT32 LOOKUP TEST");
    console_set_color(WHITE);
    console_putln("Looking for HELLO.BIN in root directory");
    console_present();

    sd_result = sdmmc0_init(&sd);
    if (!sd_result)
        mount_result = fat_mount_first(&volume);
    if (!mount_result)
        find_result = fat_find_root(&volume, hello_name, &hello);
    if (!find_result)
        read_result = fat_read_file_sector(&volume, &hello, 0U,
                                           file_sector);

    console_clear();
    console_set_color(CYAN);
    console_putln("DPOS350P FAT16/FAT32 LOOKUP TEST");
    console_putln("");

    console_status("SDC0 INIT ........", sd_result ? "FAIL" : "OK",
                   sd_result ? RED : GREEN);
    console_status("FAT MOUNT ........", mount_result ? "FAIL" : "OK",
                   mount_result ? RED : GREEN);
    console_status("FIND HELLO.BIN ...", find_result ? "FAIL" : "OK",
                   find_result ? RED : GREEN);
    console_status("READ FILE SECTOR .", read_result ? "FAIL" : "OK",
                   read_result ? RED : GREEN);

    value_line("SD ERROR ...........", (uint32_t)sd_result,
               sd_result ? RED : CYAN);
    value_line("MOUNT ERROR ........", (uint32_t)mount_result,
               mount_result ? RED : CYAN);
    value_line("FIND ERROR .........", (uint32_t)find_result,
               find_result ? RED : CYAN);
    value_line("READ ERROR .........", (uint32_t)read_result,
               read_result ? RED : CYAN);

    if (!mount_result) {
        console_putln("");
        value_line("FAT TYPE ...........", volume.fat_type, GREEN);
        value_line("PARTITION TYPE .....", volume.partition_type, CYAN);
        value_line("PARTITION LBA ......", volume.partition_lba, CYAN);
        value_line("FAT BEGIN LBA ......", volume.fat_begin_lba, CYAN);
        value_line("ROOT DIR LBA .......", volume.root_dir_lba, CYAN);
        value_line("DATA BEGIN LBA .....", volume.data_begin_lba, CYAN);
        value_line("SECTORS/CLUSTER ....", volume.sectors_per_cluster, CYAN);
    }

    if (!find_result) {
        console_putln("");
        value_line("FILE CLUSTER .......", hello.first_cluster, CYAN);
        value_line("FILE FIRST LBA .....", hello.first_sector_lba, CYAN);
        value_line("FILE SIZE ..........", hello.size, CYAN);
    }

    if (!read_result) {
        console_putln("");
        bytes_line("FIRST 16 BYTES .....", file_sector, 16U);
        value_line("FIRST ARM WORD .....", le32(file_sector), GREEN);

        console_status("EXPECTED SIZE .....",
                       hello.size == 3492U ? "3492" : "DIFF",
                       hello.size == 3492U ? GREEN : YELLOW);
        console_status("HELLO SIGNATURE ...",
                       le32(file_sector) == 0xE59FD08CU
                           ? "MATCH" : "DIFF",
                       le32(file_sector) == 0xE59FD08CU
                           ? GREEN : RED);
    }

    console_present();
    for (;;) {}
}
