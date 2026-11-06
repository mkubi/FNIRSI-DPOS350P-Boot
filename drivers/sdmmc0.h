#ifndef SDMMC0_H
#define SDMMC0_H

#include <stdint.h>

#define SDMMC0_SECTOR_SIZE 512U

typedef struct {
    uint32_t cmd8_response;
    uint32_t ocr;
    uint32_t rca;
    uint32_t high_capacity;
    uint32_t cid[4];
    uint32_t csd[4];
    uint32_t capacity_sectors;
    uint32_t decode_path;
    uint32_t decode_resp18;
    uint32_t decode_resp14;
    uint32_t decode_resp10;
    uint32_t decode_c_size;
    uint32_t decode_x200;
    uint32_t decode_x400;
    uint32_t last_cmd;
    uint32_t last_risr;
    uint32_t last_cmdr;
    uint32_t last_resp0;
    uint32_t last_star;
    uint32_t words_read;
    uint32_t ccu_gate0;
    uint32_t ccu_rst0;
    uint32_t ccu_clk;
    uint32_t pf_cfg0;
    uint32_t pf_pul0;
    uint32_t hwrst;
    uint32_t gctl;
    uint32_t ckcr;
    int last_error;
} sdmmc0_info_t;

int sdmmc0_init(sdmmc0_info_t *info);
int sdmmc0_read_sector(uint32_t sector, uint8_t *buffer);
int sdmmc0_write_sector(uint32_t sector, const uint8_t *buffer);

/*
 * Deselect the active card before handing SDC0 to stock firmware.
 *
 * This is deliberately separate from sdmmc0_init(): the custom FAT/block
 * path requires the card to remain selected after initialization.
 */
int sdmmc0_prepare_stock_handoff(void);

#endif /* SDMMC0_H */
