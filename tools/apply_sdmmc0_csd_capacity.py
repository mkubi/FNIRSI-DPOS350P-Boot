#!/usr/bin/env python3
from pathlib import Path

header_path = Path("drivers/sdmmc0.h")
source_path = Path("drivers/sdmmc0.c")

header = header_path.read_text()
source = source_path.read_text()

old = """    uint32_t rca;
    uint32_t high_capacity;
    uint32_t last_cmd;
"""
new = """    uint32_t rca;
    uint32_t high_capacity;
    uint32_t csd[4];
    uint32_t capacity_sectors;
    uint32_t last_cmd;
"""
if old in header:
    header = header.replace(old, new, 1)
    header_path.write_text(header)
    print("Patched drivers/sdmmc0.h")
elif "uint32_t capacity_sectors;" in header:
    print("drivers/sdmmc0.h: already patched")
else:
    raise SystemExit("Expected insertion point not found in drivers/sdmmc0.h")

old = """#define SD_RESP0          (SD0_BASE + 0x020U)
#define SD_RISR           (SD0_BASE + 0x038U)
"""
new = """#define SD_RESP0          (SD0_BASE + 0x020U)
#define SD_RESP1          (SD0_BASE + 0x024U)
#define SD_RESP2          (SD0_BASE + 0x028U)
#define SD_RESP3          (SD0_BASE + 0x02CU)
#define SD_RISR           (SD0_BASE + 0x038U)
"""
if old in source:
    source = source.replace(old, new, 1)
elif "#define SD_RESP3" not in source:
    raise SystemExit("Response-register insertion point not found")

old = """    mmio_write32(SD_RISR, 0xFFFFFFFFU);
    return 0;
}

int sdmmc0_init(sdmmc0_info_t *info)
"""
new = """    mmio_write32(SD_RISR, 0xFFFFFFFFU);
    return 0;
}

static int send_cmd_long(uint32_t idx, uint32_t arg, uint32_t flags,
                         uint32_t response[4])
{
    int result;

    result = send_cmd(idx, arg, flags | CMD_LONG_RESP, NULL);
    if (result)
        return result;

    response[0] = mmio_read32(SD_RESP0);
    response[1] = mmio_read32(SD_RESP1);
    response[2] = mmio_read32(SD_RESP2);
    response[3] = mmio_read32(SD_RESP3);
    return 0;
}

static uint32_t decode_capacity_sectors(const uint32_t csd[4])
{
    uint32_t csd_structure = (csd[3] >> 30) & 3U;

    if (csd_structure == 1U) {
        uint32_t c_size = ((csd[2] & 0x3FU) << 16)
                        | ((csd[1] >> 16) & 0xFFFFU);

        return (c_size + 1U) * 1024U;
    }

    return 0U;
}

int sdmmc0_init(sdmmc0_info_t *info)
"""
if old in source:
    source = source.replace(old, new, 1)
elif "decode_capacity_sectors" not in source:
    raise SystemExit("CSD-helper insertion point not found")

old = """    info->rca = 0U;
    info->high_capacity = 0U;
    info->last_cmd = 0U;
"""
new = """    info->rca = 0U;
    info->high_capacity = 0U;
    info->csd[0] = 0U;
    info->csd[1] = 0U;
    info->csd[2] = 0U;
    info->csd[3] = 0U;
    info->capacity_sectors = 0U;
    info->last_cmd = 0U;
"""
if old in source:
    source = source.replace(old, new, 1)
elif "info->capacity_sectors = 0U;" not in source:
    raise SystemExit("Info initialization insertion point not found")

old = """    card_rca = info->rca;
    card_high_capacity = info->high_capacity;

    /* CMD7: select the card and move it to transfer state. */
"""
new = """    card_rca = info->rca;
    card_high_capacity = info->high_capacity;

    /* CMD9: read CSD while the card is still in standby state. */
    r = send_cmd_long(9U, card_rca,
                      CMD_RESP | CMD_CHECK_CRC, info->csd);
    if (r) {
        info->last_error = r;
        return r;
    }

    info->capacity_sectors = decode_capacity_sectors(info->csd);

    /* CMD7: select the card and move it to transfer state. */
"""
if old in source:
    source = source.replace(old, new, 1)
elif "info->capacity_sectors = decode_capacity_sectors" not in source:
    raise SystemExit("CMD9 insertion point not found")

source_path.write_text(source)
print("Patched drivers/sdmmc0.c")
