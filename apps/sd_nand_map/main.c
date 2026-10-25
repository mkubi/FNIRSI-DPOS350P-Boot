#include <stdint.h>
#include "display.h"
#include "console.h"
#include "timer.h"
#include "sdmmc0.h"

#define BLACK 0x0000U
#define WHITE 0xFFFFU
#define GREEN 0x07E0U
#define YELLOW 0xFFE0U
#define RED 0xF800U

static uint8_t sector_buffer[SDMMC0_SECTOR_SIZE]
    __attribute__((aligned(4)));

static char line[80];

static char hex_digit(uint32_t value)
{
    value &= 0xFU;
    return value < 10U ? (char)('0' + value) : (char)('A' + value - 10U);
}

static char *append_text(char *out, const char *text)
{
    while (*text)
        *out++ = *text++;
    return out;
}

static char *append_hex32(char *out, uint32_t value)
{
    int shift;
    *out++ = '0';
    *out++ = 'x';
    for (shift = 28; shift >= 0; shift -= 4)
        *out++ = hex_digit(value >> (unsigned)shift);
    return out;
}

static char *append_dec(char *out, uint32_t value)
{
    char reverse[10];
    unsigned count = 0U;

    if (value == 0U) {
        *out++ = '0';
        return out;
    }

    while (value != 0U) {
        reverse[count++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (count != 0U)
        *out++ = reverse[--count];
    return out;
}

static void put_line(const char *text, uint16_t color)
{
    console_set_color(color);
    console_putln(text);
    console_present();
}

static uint32_t fnv1a32(const uint8_t *data, uint32_t length)
{
    uint32_t hash = 2166136261U;
    uint32_t i;
    for (i = 0U; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static const char *classify_sector(const uint8_t *data,
                                   uint32_t *zero_count,
                                   uint32_t *ff_count)
{
    uint32_t zeros = 0U;
    uint32_t ffs = 0U;
    uint32_t i;

    for (i = 0U; i < SDMMC0_SECTOR_SIZE; ++i) {
        if (data[i] == 0x00U)
            ++zeros;
        if (data[i] == 0xFFU)
            ++ffs;
    }

    *zero_count = zeros;
    *ff_count = ffs;

    if (zeros == SDMMC0_SECTOR_SIZE)
        return "ZERO";
    if (ffs == SDMMC0_SECTOR_SIZE)
        return "FF";
    return "DATA";
}

static void probe_lba(uint32_t lba)
{
    uint32_t zeros;
    uint32_t ffs;
    uint32_t hash;
    const char *kind;
    char *out = line;
    int result;

    result = sdmmc0_read_sector(lba, sector_buffer);
    out = append_text(out, "LBA ");
    out = append_dec(out, lba);
    out = append_text(out, ": ");

    if (result != 0) {
        out = append_text(out, "ERR ");
        out = append_hex32(out, (uint32_t)result);
        *out = '\0';
        put_line(line, RED);
        return;
    }

    kind = classify_sector(sector_buffer, &zeros, &ffs);
    hash = fnv1a32(sector_buffer, SDMMC0_SECTOR_SIZE);
    out = append_text(out, kind);
    out = append_text(out, " H=");
    out = append_hex32(out, hash);
    *out = '\0';
    put_line(line, GREEN);
}

int main(void)
{
    static sdmmc0_info_t info;
    static const uint32_t fixed_probes[] = {
        0U,
        1U,
        247806U,
        247807U,
        247808U,
        247809U,
        262144U,
        524288U,
        1048576U,
        1572864U,
        1900000U
    };
    char *out;
    uint32_t i;
    int result;

    display_init();
    console_init(WHITE, BLACK, 1U);
    timer_init();

    put_line("SD-NAND READ-ONLY MAP", WHITE);
    put_line("NO WRITE COMMANDS LINKED", YELLOW);

    result = sdmmc0_init(&info);
    if (result != 0) {
        out = line;
        out = append_text(out, "INIT ERROR ");
        out = append_hex32(out, (uint32_t)result);
        *out = '\0';
        put_line(line, RED);
        for (;;)
            __asm__ volatile ("wfi");
    }

    out = line;
    out = append_text(out, "OCR=");
    out = append_hex32(out, info.ocr);
    out = append_text(out, " HC=");
    out = append_dec(out, info.high_capacity);
    *out = '\0';
    put_line(line, WHITE);

    out = line;
    out = append_text(out, "CSD: ");
    out = append_hex32(out, info.csd[3]);
    *out++ = ' ';
    out = append_hex32(out, info.csd[2]);
    *out++ = ' ';
    out = append_hex32(out, info.csd[1]);
    *out++ = ' ';
    out = append_hex32(out, info.csd[0]);
    *out = '\0';
    put_line(line, WHITE);

    out = line;
    out = append_text(out, "SECTORS=");
    out = append_dec(out, info.capacity_sectors);
    out = append_text(out, " MIB=");
    out = append_dec(out, info.capacity_sectors / 2048U);
    *out = '\0';
    put_line(line, YELLOW);

    for (i = 0U; i < sizeof(fixed_probes) / sizeof(fixed_probes[0]); ++i) {
        if (info.capacity_sectors != 0U &&
            fixed_probes[i] >= info.capacity_sectors)
            continue;
        probe_lba(fixed_probes[i]);
    }

    if (info.capacity_sectors > 1U) {
        put_line("LAST VALID SECTOR:", WHITE);
        probe_lba(info.capacity_sectors - 1U);
    }

    put_line("DONE - POWER CYCLE TO EXIT", YELLOW);
    for (;;)
        __asm__ volatile ("wfi");
}
