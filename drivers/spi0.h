#ifndef DRIVERS_SPI0_H
#define DRIVERS_SPI0_H
#include <stdint.h>
int spi0_init(void);
int spi0_transfer(const uint8_t *tx, uint8_t *rx, unsigned int length);
unsigned int spi0_debug_stage(void);
#endif
