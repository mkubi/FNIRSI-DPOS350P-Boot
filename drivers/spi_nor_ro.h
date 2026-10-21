#ifndef DRIVERS_SPI_NOR_RO_H
#define DRIVERS_SPI_NOR_RO_H
#include <stdint.h>
typedef struct { uint8_t manufacturer; uint8_t memory_type; uint8_t capacity; } spi_nor_jedec_id_t;
int spi_nor_ro_init(void);
int spi_nor_read_jedec_id(spi_nor_jedec_id_t *id);
int spi_nor_read(uint32_t address, uint8_t *buffer, uint32_t length);
int spi_nor_read_chunked(uint32_t address, uint8_t *buffer, uint32_t length);
unsigned int spi_nor_debug_stage(void);
const uint8_t *spi_nor_debug_raw(void);
#endif
