#include <stdint.h>
#include "spi0.h"
#include "spi_nor_stage2_rw.h"

#define CMD_WRITE_ENABLE  0x06U
#define CMD_READ_STATUS1  0x05U
#define CMD_SECTOR_ERASE  0x20U
#define CMD_PAGE_PROGRAM  0x02U
#define STATUS_BUSY       0x01U
#define STATUS_WEL        0x02U
#define MAX_PROGRAM_DATA  60U
#define PAGE_SIZE         256U
#define STATUS_TIMEOUT    4000000U

static int read_status(uint8_t *status)
{
    const uint8_t tx[2] = { CMD_READ_STATUS1, 0xFFU };
    uint8_t rx[2];
    int result = spi0_transfer(tx, rx, sizeof(tx));
    if (result)
        return result;
    *status = rx[1];
    return 0;
}

static int wait_ready(void)
{
    uint32_t timeout;
    uint8_t status;
    int result;
    for (timeout = 0U; timeout < STATUS_TIMEOUT; ++timeout) {
        result = read_status(&status);
        if (result)
            return result;
        if ((status & STATUS_BUSY) == 0U)
            return 0;
    }
    return -20;
}

static int write_enable(void)
{
    const uint8_t command = CMD_WRITE_ENABLE;
    uint8_t status;
    int result;
    result = spi0_transfer(&command, 0, 1U);
    if (result)
        return result;
    result = read_status(&status);
    if (result)
        return result;
    return (status & STATUS_WEL) != 0U ? 0 : -21;
}

int spi_nor_stage2_rw_init(void)
{
    int result = spi0_init();
    if (result)
        return result;
    return wait_ready();
}

int spi_nor_stage2_erase_sector(uint32_t address)
{
    uint8_t tx[4];
    int result;
    if ((address & (SPI_NOR_STAGE2_SECTOR_SIZE - 1U)) != 0U)
        return -22;
    result = write_enable();
    if (result)
        return result;
    tx[0] = CMD_SECTOR_ERASE;
    tx[1] = (uint8_t)(address >> 16);
    tx[2] = (uint8_t)(address >> 8);
    tx[3] = (uint8_t)address;
    result = spi0_transfer(tx, 0, sizeof(tx));
    if (result)
        return result;
    return wait_ready();
}

int spi_nor_stage2_program(uint32_t address, const uint8_t *data,
                           uint32_t length)
{
    uint8_t tx[64];
    uint32_t chunk;
    uint32_t page_remaining;
    uint32_t i;
    int result;
    if (!data || length == 0U)
        return -23;
    while (length != 0U) {
        page_remaining = PAGE_SIZE - (address & (PAGE_SIZE - 1U));
        chunk = length;
        if (chunk > MAX_PROGRAM_DATA)
            chunk = MAX_PROGRAM_DATA;
        if (chunk > page_remaining)
            chunk = page_remaining;
        result = write_enable();
        if (result)
            return result;
        tx[0] = CMD_PAGE_PROGRAM;
        tx[1] = (uint8_t)(address >> 16);
        tx[2] = (uint8_t)(address >> 8);
        tx[3] = (uint8_t)address;
        for (i = 0U; i < chunk; ++i)
            tx[4U + i] = data[i];
        result = spi0_transfer(tx, 0, chunk + 4U);
        if (result)
            return result;
        result = wait_ready();
        if (result)
            return result;
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return 0;
}
