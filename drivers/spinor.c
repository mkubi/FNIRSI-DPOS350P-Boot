#include "spinor.h"
#include "arch/r11/regs.h"

#define SPINOR_READ_COMMAND 0x03U
#define SPI_READ_CHUNK      60U
#define SPI_TIMEOUT         1000000U
#define PC0_PC3_SPI0_MUX    0x00003333U
#define SPI0_CLOCK_DIV4     0x00001001U

static inline void write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static inline uint32_t read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static inline void write8(uint32_t address, uint8_t value)
{
    *(volatile uint8_t *)(uintptr_t)address = value;
}

static inline uint8_t read8(uint32_t address)
{
    return *(volatile uint8_t *)(uintptr_t)address;
}

static int wait_soft_reset(void)
{
    uint32_t timeout = SPI_TIMEOUT;
    while ((read32(R11_SPI0_GCR) & R11_SPI_GCR_SOFT_RESET) != 0U) {
        if (--timeout == 0U)
            return -1;
    }
    return 0;
}

static int wait_rx_count(uint32_t count)
{
    uint32_t timeout = SPI_TIMEOUT;
    while ((read32(R11_SPI0_FIFO_STA) & 0x7fU) < count) {
        if (--timeout == 0U)
            return -1;
    }
    return 0;
}

int spinor_init(void)
{
    uint32_t value;

    /* R11/V3s SPI0 boot pins: PC0..PC3. Function 3 is SPI0. */
    value = read32(R11_PC_CFG0);
    value &= ~0x0000ffffU;
    value |= PC0_PC3_SPI0_MUX;
    write32(R11_PC_CFG0, value);

    value = read32(R11_CCU_BUS_RESET0);
    write32(R11_CCU_BUS_RESET0, value | R11_SPI0_RESET_BIT);

    value = read32(R11_CCU_BUS_GATE0);
    write32(R11_CCU_BUS_GATE0, value | R11_SPI0_GATE_BIT);

    /* OSC24M module clock, controller divider /4 => 6 MHz SPI clock. */
    write32(R11_CCU_SPI0_CLK, 1U << 31);
    write32(R11_SPI0_CCTL, SPI0_CLOCK_DIV4);

    value = read32(R11_SPI0_GCR);
    value |= R11_SPI_GCR_ENABLE | R11_SPI_GCR_MASTER |
             R11_SPI_GCR_SOFT_RESET;
    write32(R11_SPI0_GCR, value);

    return wait_soft_reset();
}

static int spinor_read_chunk(uint32_t offset, uint8_t *buffer,
                             uint32_t length)
{
    uint32_t received;

    write32(R11_SPI0_MBC, 4U + length);
    write32(R11_SPI0_MTC, 4U);
    write32(R11_SPI0_BCC, 4U);

    write8(R11_SPI0_TXD, SPINOR_READ_COMMAND);
    write8(R11_SPI0_TXD, (uint8_t)(offset >> 16));
    write8(R11_SPI0_TXD, (uint8_t)(offset >> 8));
    write8(R11_SPI0_TXD, (uint8_t)offset);

    write32(R11_SPI0_TCR, read32(R11_SPI0_TCR) | R11_SPI_TCR_XCH);

    received = 4U + length;
    if (wait_rx_count(received) != 0)
        return -1;

    /* Discard command/address echo bytes from the RX FIFO. */
    (void)read32(R11_SPI0_RXD);

    while (length-- != 0U)
        *buffer++ = read8(R11_SPI0_RXD);

    return 0;
}

int spinor_read(uint32_t offset, void *buffer, uint32_t length)
{
    uint8_t *destination = (uint8_t *)buffer;

    if (buffer == 0 || length == 0U)
        return -1;
    if (offset > 0x00ffffffU || length > 0x01000000U - offset)
        return -1;

    while (length != 0U) {
        uint32_t chunk = length > SPI_READ_CHUNK ? SPI_READ_CHUNK : length;
        if (spinor_read_chunk(offset, destination, chunk) != 0)
            return -1;
        offset += chunk;
        destination += chunk;
        length -= chunk;
    }
    return 0;
}

void spinor_deinit(void)
{
    uint32_t value;

    value = read32(R11_SPI0_GCR);
    value &= ~(R11_SPI_GCR_ENABLE | R11_SPI_GCR_MASTER);
    write32(R11_SPI0_GCR, value);
    write32(R11_CCU_SPI0_CLK, 0U);

    value = read32(R11_CCU_BUS_GATE0);
    write32(R11_CCU_BUS_GATE0, value & ~R11_SPI0_GATE_BIT);

    value = read32(R11_CCU_BUS_RESET0);
    write32(R11_CCU_BUS_RESET0, value & ~R11_SPI0_RESET_BIT);
}
