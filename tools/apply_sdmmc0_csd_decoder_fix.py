#!/usr/bin/env python3
from pathlib import Path

path = Path("drivers/sdmmc0.c")
text = path.read_text()

old = '''static uint32_t decode_capacity_sectors(const uint32_t csd[4])
{
    uint32_t csd_structure = (csd[3] >> 30) & 3U;

    if (csd_structure == 1U) {
        uint32_t c_size = ((csd[2] & 0x3FU) << 16)
                        | ((csd[1] >> 16) & 0xFFFFU);

        return (c_size + 1U) * 1024U;
    }

    return 0U;
}
'''

new = '''static uint32_t decode_csd_v2_words(const uint32_t words[4])
{
    uint32_t csd_structure;
    uint32_t c_size;
    uint32_t sectors;

    /* words[0] contains CSD bits 31:0; words[3] contains bits 127:96. */
    csd_structure = (words[3] >> 30) & 3U;
    if (csd_structure != 1U)
        return 0U;

    /* SDHC/SDXC CSD v2.0: C_SIZE is bits 69:48. */
    c_size = ((words[2] & 0x3FU) << 16)
           | ((words[1] >> 16) & 0xFFFFU);

    /* Capacity in 512-byte sectors is (C_SIZE + 1) * 1024. */
    if (c_size >= 0x3FFFFFU)
        return 0U;

    sectors = (c_size + 1U) * 1024U;

    /* Reject clearly implausible results caused by response alignment/order. */
    if (sectors < 0x00020000U)
        return 0U;

    return sectors;
}

static void reverse_words(const uint32_t input[4], uint32_t output[4])
{
    output[0] = input[3];
    output[1] = input[2];
    output[2] = input[1];
    output[3] = input[0];
}

static void shift_words_right8(const uint32_t input[4], uint32_t output[4])
{
    output[0] = (input[0] >> 8) | (input[1] << 24);
    output[1] = (input[1] >> 8) | (input[2] << 24);
    output[2] = (input[2] >> 8) | (input[3] << 24);
    output[3] = input[3] >> 8;
}

static void shift_words_left8(const uint32_t input[4], uint32_t output[4])
{
    output[3] = (input[3] << 8) | (input[2] >> 24);
    output[2] = (input[2] << 8) | (input[1] >> 24);
    output[1] = (input[1] << 8) | (input[0] >> 24);
    output[0] = input[0] << 8;
}

static uint32_t decode_capacity_sectors(const uint32_t raw[4])
{
    uint32_t candidate[4];
    uint32_t shifted[4];
    uint32_t sectors;

    /* Try RESP0..RESP3 as low-to-high 32-bit CSD words. */
    sectors = decode_csd_v2_words(raw);
    if (sectors != 0U)
        return sectors;

    /* Some Allwinner response paths expose the four words in reverse order. */
    reverse_words(raw, candidate);
    sectors = decode_csd_v2_words(candidate);
    if (sectors != 0U)
        return sectors;

    /* R2 responses may include/exclude the leading CRC framing byte. */
    shift_words_right8(raw, shifted);
    sectors = decode_csd_v2_words(shifted);
    if (sectors != 0U)
        return sectors;

    shift_words_left8(raw, shifted);
    sectors = decode_csd_v2_words(shifted);
    if (sectors != 0U)
        return sectors;

    shift_words_right8(candidate, shifted);
    sectors = decode_csd_v2_words(shifted);
    if (sectors != 0U)
        return sectors;

    shift_words_left8(candidate, shifted);
    return decode_csd_v2_words(shifted);
}
'''

if old in text:
    path.write_text(text.replace(old, new, 1))
    print("Patched drivers/sdmmc0.c CSD decoder")
elif "static uint32_t decode_csd_v2_words" in text:
    print("drivers/sdmmc0.c: CSD decoder fix already applied")
else:
    raise SystemExit("Original decode_capacity_sectors() block not found")
