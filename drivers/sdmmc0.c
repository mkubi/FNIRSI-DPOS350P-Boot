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
#define CMD_SEND_INIT     (1U << 15)
#define CMD_WAIT_PRE      (1U << 13)
#define CMD_UPCLK_ONLY    (1U << 21)
#define CMD_START         (1U << 31)
#define RINT_CMD_DONE     (1U << 2)
#define RINT_DATA_OVER    (1U << 3)
#define RINT_RX_DATA_REQ  (1U << 5)
#define STAR_FIFO_EMPTY   (1U << 2)
#define RINT_ERROR_MASK   0x0000BBC2U
#define TIMEOUT           1000000U

/* Keep identification at 400 kHz, then use the HW-tested 24 MHz data clock. */
#define SDMMC0_IDENT_CLOCK_REG 0x8002000EU
#define SDMMC0_DATA_CLOCK_REG  0x80000000U

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

    mmio_write32(SD_CKCR, 1U << 16);
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

static uint32_t decode_csd_v2_words(const uint32_t words[4])
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
    info->csd[0] = 0U;
    info->csd[1] = 0U;
    info->csd[2] = 0U;
    info->csd[3] = 0U;
    info->capacity_sectors = 0U;
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

    r = send_cmd(2U, 0U,
                 CMD_RESP | CMD_LONG_RESP | CMD_CHECK_CRC, &resp);
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

    /* Standard-capacity cards use byte addressing and require CMD16. */
    if (!card_high_capacity) {
        r = send_cmd(16U, SDMMC0_SECTOR_SIZE,
                     CMD_RESP | CMD_CHECK_CRC, &resp);
        if (r) {
            info->last_error = r;
            return r;
        }
    }

    /* Switch to the stress-tested 24 MHz data clock. */
    r = set_card_clock_reg(SDMMC0_DATA_CLOCK_REG);
    if (r) {
        /* Recovery-first fallback to the proven 400 kHz baseline. */
        (void)set_card_clock_reg(SDMMC0_IDENT_CLOCK_REG);
    }

    capture_state();
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
