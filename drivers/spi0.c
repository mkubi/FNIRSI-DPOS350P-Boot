#include <stdint.h>
#include "spi0.h"
#include "gpio.h"
#include "regs.h"

/* Allwinner R11/V3s SPI0, confirmed on DPOS350P: PC0..PC3. */
#define CCU_BASE             0x01C20000U
#define CCU_BUS_CLK_GATE0    (CCU_BASE + 0x060U)
#define CCU_SPI0_CLK         (CCU_BASE + 0x0A0U)
#define CCU_BUS_SOFT_RST0    (CCU_BASE + 0x2C0U)
#define SPI0_BASE            0x01C68000U

#define SPI_GCR              (SPI0_BASE + 0x04U)
#define SPI_TCR              (SPI0_BASE + 0x08U)
#define SPI_IER              (SPI0_BASE + 0x10U)
#define SPI_ISR              (SPI0_BASE + 0x14U)
#define SPI_FCR              (SPI0_BASE + 0x18U)
#define SPI_FSR              (SPI0_BASE + 0x1CU)
#define SPI_CCR              (SPI0_BASE + 0x24U)
#define SPI_MBC              (SPI0_BASE + 0x30U)
#define SPI_MTC              (SPI0_BASE + 0x34U)
#define SPI_BCC              (SPI0_BASE + 0x38U)
#define SPI_TXD              (SPI0_BASE + 0x200U)
#define SPI_RXD              (SPI0_BASE + 0x300U)

#define SPI_GCR_EN           (1U << 0)
#define SPI_GCR_MASTER       (1U << 1)
#define SPI_GCR_TP           (1U << 7)
#define SPI_GCR_SRST         (1U << 31)
#define SPI_TCR_CS_MANUAL    (1U << 6)
#define SPI_TCR_CS_LEVEL     (1U << 7)
#define SPI_TCR_SAMPLE_MODE  (1U << 13)
#define SPI_TCR_XCH          (1U << 31)
#define SPI_ISR_TC           (1U << 12)
#define SPI_FCR_RX_RST       (1U << 15)
#define SPI_FCR_TX_RST       (1U << 31)
#define SPI_CCR_DRS          (1U << 12)

#define TIMEOUT              1000000U

#define SET_STAGE(n) do { (void)(n); } while(0)

static void cs_set(unsigned int high)
{
    uint32_t v = mmio_read32(SPI_TCR);
    if (high) v |= SPI_TCR_CS_LEVEL;
    else      v &= ~SPI_TCR_CS_LEVEL;
    mmio_write32(SPI_TCR, v);
}

int spi0_init(void)
{
    uint32_t v;

    /* PC0=CLK, PC1=CS0, PC2=MISO, PC3=MOSI; V3s SPI0 function is mux 3. */
    gpio_set_function(GPIO_PORT_C, 0U, 3U);
    gpio_set_function(GPIO_PORT_C, 1U, 3U);
    gpio_set_function(GPIO_PORT_C, 2U, 3U);
    gpio_set_function(GPIO_PORT_C, 3U, 3U);

    /* Bus gate and reset for SPI0 are bit 20. */
    v = mmio_read32(CCU_BUS_CLK_GATE0);
    mmio_write32(CCU_BUS_CLK_GATE0, v | (1U << 20));
    v = mmio_read32(CCU_BUS_SOFT_RST0);
    mmio_write32(CCU_BUS_SOFT_RST0, v & ~(1U << 20));
    mmio_write32(CCU_BUS_SOFT_RST0, v |  (1U << 20));

    /* OSC24M parent, module clock enabled. Controller divider below gives 6MHz. */
    mmio_write32(CCU_SPI0_CLK, 1U << 31);

    mmio_write32(SPI_GCR, SPI_GCR_SRST);
    for (v = 0U; v < TIMEOUT; ++v)
        if ((mmio_read32(SPI_GCR) & SPI_GCR_SRST) == 0U) break;
    if (v == TIMEOUT) return -1;

    mmio_write32(SPI_GCR, SPI_GCR_EN | SPI_GCR_MASTER | SPI_GCR_TP);
    /* Mode 0, CS0, manual CS, MSB first, normal sampling at <=24MHz. */
    mmio_write32(SPI_TCR, SPI_TCR_CS_MANUAL | SPI_TCR_CS_LEVEL |
                          SPI_TCR_SAMPLE_MODE);
    /* SPI_CLK = 24MHz / (2 * (1 + 1)) = 6MHz. */
    mmio_write32(SPI_CCR, SPI_CCR_DRS | 1U);
    mmio_write32(SPI_IER, 0U);
    mmio_write32(SPI_ISR, 0xFFFFFFFFU);
    mmio_write32(SPI_FCR, SPI_FCR_RX_RST | SPI_FCR_TX_RST);
    return 0;
}

int spi0_transfer(const uint8_t *tx, uint8_t *rx, unsigned int length)
{
    unsigned int i, timeout, rx_count;
    uint32_t tcr;
    if (length == 0U || length > 64U) return -1;

    SET_STAGE(1U); /* FIFO reset/status clear */
    mmio_write32(SPI_FCR, SPI_FCR_RX_RST | SPI_FCR_TX_RST);
    mmio_write32(SPI_ISR, 0xFFFFFFFFU);
    SET_STAGE(2U); /* transfer counters */
    mmio_write32(SPI_MBC, length);
    mmio_write32(SPI_MTC, length);
    mmio_write32(SPI_BCC, length);

    SET_STAGE(3U); /* fill TX FIFO */
    for (i = 0U; i < length; ++i)
        *(volatile uint8_t *)SPI_TXD = tx ? tx[i] : 0xFFU;

    SET_STAGE(4U); /* assert CS */
    cs_set(0U);
    tcr = mmio_read32(SPI_TCR);
    SET_STAGE(5U); /* start exchange */
    mmio_write32(SPI_TCR, tcr | SPI_TCR_XCH);

    SET_STAGE(6U); /* wait for transfer-complete */
    for (timeout = 0U; timeout < TIMEOUT; ++timeout)
        if (mmio_read32(SPI_ISR) & SPI_ISR_TC) break;
    if (timeout == TIMEOUT) { cs_set(1U); return -2; }

    SET_STAGE(7U); /* inspect RX FIFO count */
    mmio_write32(SPI_ISR, SPI_ISR_TC);
    rx_count = mmio_read32(SPI_FSR) & 0xFFU;
    if (rx_count < length) { cs_set(1U); return -3; }

    SET_STAGE(8U); /* drain RX FIFO */
    for (i = 0U; i < length; ++i) {
        uint8_t b = *(volatile uint8_t *)SPI_RXD;
        if (rx) rx[i] = b;
    }
    cs_set(1U);
    SET_STAGE(9U); /* complete */
    return 0;
}
