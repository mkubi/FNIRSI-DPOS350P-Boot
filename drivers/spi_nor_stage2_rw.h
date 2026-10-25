#ifndef SPI_NOR_STAGE2_RW_H
#define SPI_NOR_STAGE2_RW_H

#include <stdint.h>

#define SPI_NOR_STAGE2_SECTOR_SIZE 0x1000U

int spi_nor_stage2_rw_init(void);
int spi_nor_stage2_erase_sector(uint32_t address);
int spi_nor_stage2_program(uint32_t address, const uint8_t *data,
                           uint32_t length);

#endif
