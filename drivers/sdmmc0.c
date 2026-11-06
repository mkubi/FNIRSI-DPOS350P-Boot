#include <stddef.h>
#include <stdint.h>
#include "sdmmc0.h"
#include "gpio.h"
#include "regs.h"

#define CCU_BASE          0x01C20000U
#define CCU_GATE0         (CCU_BASE + 0x060U)
#define CCU_SD0_CLK       (CCU_BASE + 0x088U)
#define CCU_RST0          (CCU_BASE + 0x2C0U)

#define PIO_BASE          0x01C20800U
#define PF_CFG0           (PIO_BASE + 0x0B4U)
#define PF_PUL0           (PIO_BASE + 0x0D0U)
#define PF_PULL_MASK      0x00000FFFU
#define PF_PULL_UP        0x00000555U

#define SD0_BASE          0x01C0F000U
#define SD_GCTL           (SD0_BASE + 0x000U)
#define SD_CKCR           (SD0_BASE + 0x004U)
#define SD_TMOR           (SD0_BASE + 0x008U)
#define SD_BWDR           (SD0_BASE + 0x00CU)
#define SD_BKSR           (SD0_BASE + 0x010U)
#define SD_BYCR           (SD0_BASE + 0x014U)
#define SD_CMDR           (SD0_BASE + 0x018U)
#define SD_CAGR           (SD0_BASE + 0x01CU)
#define SD_RESP0          (SD0_BASE + 0x020U)
#define SD_RESP1          (SD0_BASE + 0x024U)
#define SD_RESP2          (SD0_BASE + 0x028U)
#define SD_RESP3          (SD0_BASE + 0x02CU)
#define SD_RISR           (SD0_BASE + 0x038U)
#define SD_STAR           (SD0_BASE + 0x03CU)
#define SD_FIFO           (SD0_BASE + 0x200U)
#define SD_HWRST          (SD0_BASE + 0x078U)

#define GCTL_FIFO_RST     (1U << 1)
#define GCTL_FIFO_ACCESS_AHB (1U << 31)
#define GCTL_RST_ALL      0x00000007U
#define CMD_RESP          (1U << 6)
#define CMD_LONG_RESP     (1U << 7)
#define CMD_CHECK_CRC     (1U << 8)
#define CMD_DATA_EXP      (1U << 9)
#define CMD_WRITE         (1U << 10)
#define CMD_SEND_INIT     (1U << 15)
#define CMD_WAIT_PRE      (1U << 13)
#define CMD_UPCLK_ONLY    (1U << 21)
#define CMD_START         (1U << 31)
#define RINT_CMD_DONE     (1U << 2)
#define RINT_DATA_OVER    (1U << 3)
#define RINT_RX_DATA_REQ  (1U << 5)
#define STAR_FIFO_EMPTY   (1U << 2)
#define STAR_FIFO_FULL    (1U << 3)
#define RINT_ERROR_MASK   0x0000BBC2U
#define TIMEOUT           1000000U

/* Keep identification at 400 kHz, then use the HW-tested 24 MHz data clock. */
#define SDMMC0_IDENT_CLOCK_REG 0x8002000EU
#define SDMMC0_DATA_CLOCK_REG  0x8140030BU
#define SDMMC0_DATA_CKCR       0x00030000U

static sdmmc0_info_t *diag_info;
static uint32_t card_rca;
static uint32_t card_high_capacity;

static void delay(unsigned n)
{
    volatile unsigned i;

    while (n--)
        for (i = 0; i < 40000U; i++)
            __asm__ volatile("nop");
}

static void capture_state(void)
{
    if (!diag_info)
        return;

    diag_info->ccu_gate0 = mmio_read32(CCU_GATE0);
    diag_info->ccu_rst0  = mmio_read32(CCU_RST0);
    diag_info->ccu_clk   = mmio_read32(CCU_SD0_CLK);
    diag_info->pf_cfg0   = mmio_read32(PF_CFG0);
    diag_info->pf_pul0   = mmio_read32(PF_PUL0);
    diag_info->hwrst     = mmio_read32(SD_HWRST);
    diag_info->gctl      = mmio_read32(SD_GCTL);
    diag_info->ckcr      = mmio_read32(SD_CKCR);
}

static int update_clock(void)
{
    unsigned t;

    mmio_write32(SD_RISR, 0xFFFFFFFFU);
    mmio_write32(SD_CMDR,
                 CMD_START | CMD_UPCLK_ONLY | CMD_WAIT_PRE);

    for (t = 0; t < TIMEOUT; t++) {
        if (!(mmio_read32(SD_CMDR) & CMD_START)) {
            mmio_write32(SD_RISR, 0xFFFFFFFFU);
            return 0;
        }
    }

    if (diag_info) {
        diag_info->last_cmdr = mmio_read32(SD_CMDR);
        diag_info->last_risr = mmio_read32(SD_RISR);
        capture_state();
    }
    return -1;
}

static int set_card_clock_reg(uint32_t ccu_clock_reg)
{
    int r;

    mmio_write32(SD_CKCR, 0U);
    r = update_clock();
    if (r)
        return -1;

    mmio_write32(CCU_SD0_CLK, ccu_clock_reg);
    delay(1U);

    mmio_write32(SD_CKCR, SDMMC0_DATA_CKCR);
    r = update_clock();
    if (r)
        return -2;

    return 0;
}

static int send_cmd(uint32_t idx, uint32_t arg, uint32_t flags,
                    uint32_t *resp)
{
    unsigned t;
    uint32_t status;

    if (diag_info)
        diag_info->last_cmd = idx;

    mmio_write32(SD_RISR, 0xFFFFFFFFU);
    mmio_write32(SD_CAGR, arg);
    mmio_write32(SD_CMDR, CMD_START | idx | flags);

    for (t = 0; t < TIMEOUT; t++) {
        status = mmio_read32(SD_RISR);
        if (status & RINT_ERROR_MASK) {
            if (diag_info) {
                diag_info->last_risr = status;
                diag_info->last_cmdr = mmio_read32(SD_CMDR);
                diag_info->last_resp0 = mmio_read32(SD_RESP0);
                capture_state();
            }
            mmio_write32(SD_RISR, status);
            return -2;
        }
        if (status & RINT_CMD_DONE)
            break;
    }

    if (t == TIMEOUT) {
        if (diag_info) {
            diag_info->last_risr = mmio_read32(SD_RISR);
            diag_info->last_cmdr = mmio_read32(SD_CMDR);
            diag_info->last_resp0 = mmio_read32(SD_RESP0);
            capture_state();
        }
        return -3;
    }

    if (resp)
        *resp = mmio_read32(SD_RESP0);

    mmio_write32(SD_RISR, 0xFFFFFFFFU);
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
    uint32_t resp_18 = csd[3];
    uint32_t resp_14 = csd[2];
    uint32_t resp_10 = csd[1];
    uint32_t read_bl_len;
    uint32_t c_size_mult;
    uint32_t c_size;
    uint32_t blocks;

    /*
     * Mirror stock 0x40068134 exactly for the response-word layout used by
     * the V3s SDC controller.  Stock reads the CMD9 response structure as:
     *   +0x18 = RESP0, +0x14 = RESP1, +0x10 = RESP2, +0x0c = RESP3.
     * send_cmd_long() stores controller RESP0..RESP3 as csd[0..3].
     * Stock command state places RESP0 at +0x0c through RESP3 at +0x18,
     * so the capacity parser sees +0x18/+0x14/+0x10 as csd[3]/[2]/[1].
     */
    if (diag_info) {
        diag_info->decode_resp18 = resp_18;
        diag_info->decode_resp14 = resp_14;
        diag_info->decode_resp10 = resp_10;
    }

    if ((resp_18 & 0xC0000000U) != 0U && card_high_capacity != 3U) {
        /* Stock modern-CSD branch @ 0x400681B0..0x400681C8. */
        c_size = (((resp_14 & 0x3FU) << 16) | (resp_10 >> 16)) + 1U;
        if (diag_info) {
            diag_info->decode_path = 2U;
            diag_info->decode_c_size = c_size;
            diag_info->decode_x200 = c_size << 9;
            diag_info->decode_x400 = c_size << 10;
        }
        return c_size << 10;
    }

    /* Stock legacy-CSD branch @ 0x400681CC..0x40068208. */
    c_size = (((resp_14 & 0x3FFU) << 2) | (resp_10 >> 30)) + 1U;
    c_size_mult = ((resp_10 >> 15) & 7U) + 2U;
    read_bl_len = (resp_14 >> 16) & 0xFU;
    blocks = (1U << read_bl_len) * (1U << c_size_mult) * c_size;

    if (diag_info) {
        diag_info->decode_path = 1U;
        diag_info->decode_c_size = c_size;
        diag_info->decode_x200 = blocks >> 10;
        diag_info->decode_x400 = blocks >> 9;
    }
    return blocks >> 9;
}

int sdmmc0_init(sdmmc0_info_t *info)
{
    uint32_t v, resp = 0U, ocr = 0U, rca = 0U;
    unsigned pin, tries;
    int r;

    if (!info)
        return -1;

    info->cmd8_response = 0U;
    info->ocr = 0U;
    info->rca = 0U;
    info->high_capacity = 0U;
    info->cid[0] = 0U;
    info->cid[1] = 0U;
    info->cid[2] = 0U;
    info->cid[3] = 0U;
    info->csd[0] = 0U;
    info->csd[1] = 0U;
    info->csd[2] = 0U;
    info->csd[3] = 0U;
    info->capacity_sectors = 0U;
    info->decode_path = 0U;
    info->decode_resp18 = 0U;
    info->decode_resp14 = 0U;
    info->decode_resp10 = 0U;
    info->decode_c_size = 0U;
    info->decode_x200 = 0U;
    info->decode_x400 = 0U;
    info->last_cmd = 0U;
    info->last_risr = 0U;
    info->last_cmdr = 0U;
    info->last_resp0 = 0U;
    info->last_star = 0U;
    info->words_read = 0U;
    info->ccu_gate0 = 0U;
    info->ccu_rst0 = 0U;
    info->ccu_clk = 0U;
    info->pf_cfg0 = 0U;
    info->pf_pul0 = 0U;
    info->hwrst = 0U;
    info->gctl = 0U;
    info->ckcr = 0U;
    info->last_error = 0;
    diag_info = info;
    card_rca = 0U;
    card_high_capacity = 0U;

    /* Use the now verified GPIO helper for a consistent driver stack. */
    for (pin = 0U; pin < 6U; pin++)
        gpio_set_function(GPIO_PORT_F, pin, 2U);

    v = mmio_read32(PF_PUL0);
    v = (v & ~PF_PULL_MASK) | PF_PULL_UP;
    mmio_write32(PF_PUL0, v);

    v = mmio_read32(CCU_GATE0);
    mmio_write32(CCU_GATE0, v | (1U << 8));
    v = mmio_read32(CCU_RST0);
    mmio_write32(CCU_RST0, v | (1U << 8));
    delay(20U);

    mmio_write32(CCU_SD0_CLK, SDMMC0_IDENT_CLOCK_REG);
    delay(1U);

    v = mmio_read32(SD_HWRST);
    mmio_write32(SD_HWRST, v | (1U << 1));
    v = mmio_read32(SD_HWRST);
    mmio_write32(SD_HWRST, v & ~(1U << 1));
    delay(5U);
    v = mmio_read32(SD_HWRST);
    mmio_write32(SD_HWRST, v | (1U << 1));
    delay(1U);

    v = mmio_read32(SD_GCTL);
    mmio_write32(SD_GCTL, v | GCTL_RST_ALL);
    delay(1U);
    v = mmio_read32(SD_GCTL);
    mmio_write32(SD_GCTL, v & ~(1U << 8));

    mmio_write32(SD_BWDR, 0U);
    mmio_write32(SD_TMOR, 0xFFFFFFFFU);
    mmio_write32(SD_CKCR, 0U);
    if (update_clock()) {
        info->last_error = -10;
        return -10;
    }

    mmio_write32(SD_CKCR, 1U << 16);
    if (update_clock()) {
        info->last_error = -11;
        return -11;
    }
    delay(2U);

    r = send_cmd(0U, 0U, CMD_SEND_INIT, NULL);
    if (r) {
        info->last_error = r;
        return r;
    }
    delay(2U);

    r = send_cmd(8U, 0x155U, CMD_RESP | CMD_CHECK_CRC, &resp);
    if (r) {
        info->last_error = r;
        return r;
    }
    info->cmd8_response = resp;
    if ((resp & 0xFFFU) != 0x155U) {
        info->last_error = -8;
        return -8;
    }

    for (tries = 0U; tries < 1000U; tries++) {
        r = send_cmd(55U, 0U, CMD_RESP | CMD_CHECK_CRC, &resp);
        if (r) {
            info->last_error = r;
            return r;
        }

        r = send_cmd(41U, 0x40FF8000U, CMD_RESP, &ocr);
        if (r) {
            info->last_error = r;
            return r;
        }

        if (ocr & 0x80000000U)
            break;
        delay(1U);
    }

    if (tries == 1000U) {
        info->last_error = -41;
        return -41;
    }

    info->ocr = ocr;
    info->high_capacity = (ocr >> 30) & 1U;

    /* CMD2: capture the complete 136-bit CID response as four controller words. */
    r = send_cmd_long(2U, 0U,
                      CMD_RESP | CMD_CHECK_CRC, info->cid);
    if (r) {
        info->last_error = r;
        return r;
    }

    r = send_cmd(3U, 0U, CMD_RESP | CMD_CHECK_CRC, &rca);
    if (r) {
        info->last_error = r;
        return r;
    }

    info->rca = rca & 0xFFFF0000U;
    card_rca = info->rca;
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
    r = send_cmd(7U, card_rca,
                 CMD_RESP | CMD_CHECK_CRC | CMD_WAIT_PRE, &resp);
    if (r) {
        info->last_error = r;
        return r;
    }

    /*
     * S0.6: reproduce the stock 0x4006840C data-mode finalization that is
     * required by the stock sector reader used behind USB MSC.
     *
     * STATIC-CONFIRMED stock order for the active card path:
     *   CMD55(RCA), CMD6(arg=2), BWDR=1, CMD16(512), CMD7(arg=0).
     * The first two commands switch the card before the controller is moved
     * to 4-bit mode.  Do not write BWDR=1 without the card-side command.
     */
    r = send_cmd(55U, card_rca,
                 CMD_RESP | CMD_CHECK_CRC, &resp);
    if (r) {
        info->last_error = -55;
        return -55;
    }

    r = send_cmd(6U, 2U,
                 CMD_RESP | CMD_CHECK_CRC, &resp);
    if (r) {
        info->last_error = -56;
        return -56;
    }

    mmio_write32(SD_BWDR, 1U);

    /* Match the HW-confirmed stock final SDC0 clock fingerprint. */
    r = set_card_clock_reg(SDMMC0_DATA_CLOCK_REG);
    if (r) {
        info->last_error = -57;
        return -57;
    }

    r = send_cmd(16U, SDMMC0_SECTOR_SIZE,
                 CMD_RESP | CMD_CHECK_CRC, &resp);
    if (r) {
        info->last_error = -58;
        return -58;
    }

    /*
     * Keep the card selected in TRANSFER state for the canonical custom
     * block-device path.  The Stage 2 updater calls FAT sector I/O directly
     * after sdmmc0_init(), so do not issue the stock handoff CMD7(0) here.
     */
    capture_state();
    return 0;
}

int sdmmc0_prepare_stock_handoff(void)
{
    int r;

    /*
     * Stock handoff finalization.
     *
     * CMD7 with argument 0 deselects the currently selected card.
     * This helper intentionally does not depend on card_rca or diag_info:
     * FATDIAG is a separate Stage 3 executable and therefore does not
     * inherit the Stage 2 driver's C static state.
     *
     * Do not move this command into sdmmc0_init().  The custom FAT/block
     * path requires the card to remain selected after initialization.
     */
    r = send_cmd(7U, 0U, 0U, NULL);
    if (r)
        return -59;

    return 0;
}

int sdmmc0_read_sector(uint32_t sector, uint8_t *buffer)
{
    uint32_t arg;
    uint32_t status;
    uint32_t *buffer32;
    unsigned words;
    unsigned t;

    if (!buffer || !card_rca)
        return -1;

    if (((uintptr_t)buffer & 3U) != 0U)
        return -16;

    buffer32 = (uint32_t *)buffer;
    arg = card_high_capacity ? sector : sector * SDMMC0_SECTOR_SIZE;

    if (diag_info) {
        diag_info->last_cmd = 17U;
        diag_info->last_risr = 0U;
        diag_info->last_cmdr = 0U;
        diag_info->last_resp0 = 0U;
        diag_info->last_star = 0U;
        diag_info->words_read = 0U;
    }

    mmio_write32(SD_RISR, 0xFFFFFFFFU);

    mmio_write32(SD_GCTL, mmio_read32(SD_GCTL) | GCTL_FIFO_RST);
    for (t = 0U; t < TIMEOUT; t++) {
        if (!(mmio_read32(SD_GCTL) & GCTL_FIFO_RST))
            break;
    }
    if (t == TIMEOUT)
        return -17;

    /* The working 1013D PIO path explicitly gives FIFO access to AHB. */
    mmio_write32(SD_GCTL,
                 mmio_read32(SD_GCTL) | GCTL_FIFO_ACCESS_AHB);

    mmio_write32(SD_BKSR, SDMMC0_SECTOR_SIZE);
    mmio_write32(SD_BYCR, SDMMC0_SECTOR_SIZE);
    mmio_write32(SD_CAGR, arg);
    mmio_write32(SD_CMDR, CMD_START | 17U | CMD_RESP | CMD_CHECK_CRC |
                          CMD_DATA_EXP | CMD_WAIT_PRE);

    /* Poll FIFO_EMPTY before every 32-bit FIFO read. */
    for (words = 0U; words < 128U; words++) {
        for (t = 0U; t < TIMEOUT; t++) {
            status = mmio_read32(SD_RISR);
            if (status & RINT_ERROR_MASK) {
                if (diag_info) {
                    diag_info->last_risr = status;
                    diag_info->last_cmdr = mmio_read32(SD_CMDR);
                    diag_info->last_resp0 = mmio_read32(SD_RESP0);
                    diag_info->last_star = mmio_read32(SD_STAR);
                    diag_info->words_read = words;
                    diag_info->last_error = -18;
                    capture_state();
                }
                mmio_write32(SD_RISR, status);
                return -18;
            }

            if (!(mmio_read32(SD_STAR) & STAR_FIFO_EMPTY))
                break;
        }

        if (t == TIMEOUT) {
            if (diag_info) {
                diag_info->last_risr = mmio_read32(SD_RISR);
                diag_info->last_cmdr = mmio_read32(SD_CMDR);
                diag_info->last_resp0 = mmio_read32(SD_RESP0);
                diag_info->last_star = mmio_read32(SD_STAR);
                diag_info->words_read = words;
                diag_info->last_error = -19;
                capture_state();
            }
            return -19;
        }

        buffer32[words] = mmio_read32(SD_FIFO);
    }

    /* Payload drained; wait for command and data completion. */
    for (t = 0U; t < TIMEOUT; t++) {
        status = mmio_read32(SD_RISR);

        if (status & RINT_ERROR_MASK) {
            if (diag_info) {
                diag_info->last_risr = status;
                diag_info->last_cmdr = mmio_read32(SD_CMDR);
                diag_info->last_resp0 = mmio_read32(SD_RESP0);
                diag_info->last_star = mmio_read32(SD_STAR);
                diag_info->words_read = 128U;
                diag_info->last_error = -20;
                capture_state();
            }
            mmio_write32(SD_RISR, status);
            return -20;
        }

        if ((status & (RINT_CMD_DONE | RINT_DATA_OVER)) ==
            (RINT_CMD_DONE | RINT_DATA_OVER)) {
            if (diag_info) {
                diag_info->last_risr = status;
                diag_info->last_cmdr = mmio_read32(SD_CMDR);
                diag_info->last_resp0 = mmio_read32(SD_RESP0);
                diag_info->last_star = mmio_read32(SD_STAR);
                diag_info->words_read = 128U;
                diag_info->last_error = 0;
                capture_state();
            }
            mmio_write32(SD_RISR, 0xFFFFFFFFU);
            return 0;
        }
    }

    if (diag_info) {
        diag_info->last_risr = mmio_read32(SD_RISR);
        diag_info->last_cmdr = mmio_read32(SD_CMDR);
        diag_info->last_resp0 = mmio_read32(SD_RESP0);
        diag_info->last_star = mmio_read32(SD_STAR);
        diag_info->words_read = 128U;
        diag_info->last_error = -21;
        capture_state();
    }
    return -21;
}


static int sdmmc0_wait_transfer_ready(void)
{
    uint32_t response;
    uint32_t state;
    unsigned t;
    int r;

    for (t = 0U; t < TIMEOUT; ++t) {
        r = send_cmd(13U, card_rca,
                     CMD_RESP | CMD_CHECK_CRC, &response);
        if (r)
            return -30;

        state = (response >> 9) & 0x0FU;
        if ((response & (1U << 8)) != 0U && state == 4U)
            return 0;
    }

    return -31;
}

int sdmmc0_write_sector(uint32_t sector, const uint8_t *buffer)
{
    uint32_t arg;
    uint32_t status;
    const uint32_t *buffer32;
    unsigned words;
    unsigned t;
    int r;

    if (!buffer || !card_rca)
        return -1;
    if (((uintptr_t)buffer & 3U) != 0U)
        return -16;

    buffer32 = (const uint32_t *)buffer;
    arg = card_high_capacity ? sector : sector * SDMMC0_SECTOR_SIZE;

    if (diag_info) {
        diag_info->last_cmd = 24U;
        diag_info->last_risr = 0U;
        diag_info->last_cmdr = 0U;
        diag_info->last_resp0 = 0U;
        diag_info->last_star = 0U;
        diag_info->words_read = 0U;
    }

    mmio_write32(SD_RISR, 0xFFFFFFFFU);
    mmio_write32(SD_GCTL, mmio_read32(SD_GCTL) | GCTL_FIFO_RST);
    for (t = 0U; t < TIMEOUT; ++t) {
        if ((mmio_read32(SD_GCTL) & GCTL_FIFO_RST) == 0U)
            break;
    }
    if (t == TIMEOUT)
        return -32;

    mmio_write32(SD_GCTL,
                 mmio_read32(SD_GCTL) | GCTL_FIFO_ACCESS_AHB);
    mmio_write32(SD_BKSR, SDMMC0_SECTOR_SIZE);
    mmio_write32(SD_BYCR, SDMMC0_SECTOR_SIZE);
    mmio_write32(SD_CAGR, arg);
    mmio_write32(SD_CMDR, CMD_START | 24U | CMD_RESP | CMD_CHECK_CRC |
                          CMD_DATA_EXP | CMD_WRITE | CMD_WAIT_PRE);

    for (words = 0U; words < 128U; ++words) {
        for (t = 0U; t < TIMEOUT; ++t) {
            status = mmio_read32(SD_RISR);
            if ((status & RINT_ERROR_MASK) != 0U) {
                mmio_write32(SD_RISR, status);
                return -33;
            }
            if ((mmio_read32(SD_STAR) & STAR_FIFO_FULL) == 0U)
                break;
        }
        if (t == TIMEOUT)
            return -34;

        mmio_write32(SD_FIFO, buffer32[words]);
    }

    for (t = 0U; t < TIMEOUT; ++t) {
        status = mmio_read32(SD_RISR);
        if ((status & RINT_ERROR_MASK) != 0U) {
            mmio_write32(SD_RISR, status);
            return -35;
        }
        if ((status & (RINT_CMD_DONE | RINT_DATA_OVER)) ==
            (RINT_CMD_DONE | RINT_DATA_OVER)) {
            mmio_write32(SD_RISR, 0xFFFFFFFFU);
            r = sdmmc0_wait_transfer_ready();
            if (r)
                return r;
            if (diag_info) {
                diag_info->last_risr = status;
                diag_info->last_cmdr = mmio_read32(SD_CMDR);
                diag_info->last_resp0 = mmio_read32(SD_RESP0);
                diag_info->last_star = mmio_read32(SD_STAR);
                diag_info->words_read = 128U;
                diag_info->last_error = 0;
                capture_state();
            }
            return 0;
        }
    }

    return -36;
}
