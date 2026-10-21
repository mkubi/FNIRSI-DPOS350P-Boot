#include <stdint.h>
#include "spi0.h"
#include "spi_nor_ro.h"

//uint8_t nor_raw[4];

int spi_nor_ro_init(void)
{
    return spi0_init();
}

int spi_nor_read_jedec_id(spi_nor_jedec_id_t *id)
{
    const uint8_t tx[4] = {
        0x9FU,
        0xFFU,
        0xFFU,
        0xFFU
    };
    uint8_t rx[4];
    int r;

    if (!id)
        return -1;

    r = spi0_transfer(tx, rx, 4U);

    if (r)
        return r;

    id->manufacturer = rx[1];
    id->memory_type  = rx[2];
    id->capacity     = rx[3];

    if ((id->manufacturer == 0U &&
         id->memory_type  == 0U &&
         id->capacity     == 0U) ||
        (id->manufacturer == 0xFFU &&
         id->memory_type  == 0xFFU &&
         id->capacity     == 0xFFU))
        return -4;

    return 0;
}


int spi_nor_read(uint32_t address, uint8_t *buffer, uint32_t length)
{
    /* Current spi0_transfer() is limited to 64 bytes total. 03h consumes
     * four bytes (opcode + 24-bit address), leaving up to 60 data bytes. */
    uint8_t tx[64];
    uint8_t rx[64];
    uint32_t total;
    uint32_t i;
    int r;

    if (!buffer || length == 0U || length > 60U)
        return -1;
    if (address > 0x1FFFFFU || (address + length) > 0x200000U)
        return -2;

    total = length + 4U;
    tx[0] = 0x03U;
    tx[1] = (uint8_t)(address >> 16);
    tx[2] = (uint8_t)(address >> 8);
    tx[3] = (uint8_t)address;
    for (i = 4U; i < total; ++i)
        tx[i] = 0xFFU;

    r = spi0_transfer(tx, rx, total);
    if (r)
        return r;

    for (i = 0U; i < length; ++i)
        buffer[i] = rx[i + 4U];
    return 0;
}


int spi_nor_read_chunked(uint32_t address, uint8_t *buffer, uint32_t length)
{
    uint32_t chunk;
    int result;

    if (!buffer || length == 0U)
        return -1;
    if (address >= 0x200000U || length > (0x200000U - address))
        return -2;

    while (length != 0U) {
        chunk = (length > 60U) ? 60U : length;
        result = spi_nor_read(address, buffer, chunk);
        if (result)
            return result;

        address += chunk;
        buffer  += chunk;
        length  -= chunk;
    }
    return 0;
}
