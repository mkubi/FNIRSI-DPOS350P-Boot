#ifndef FPGA_PROTOCOL_H
#define FPGA_PROTOCOL_H

#include <stdint.h>

/*
 * DPOS350P FPGA protocol framing over the low-level PE bus.
 *
 * These helpers own selector framing and byte order only. Higher-level
 * acquisition, SMEC and capture semantics are added in later refactor steps.
 */
void fpga_protocol_write_u8(uint8_t selector, uint8_t value);
void fpga_protocol_write_u16_be(uint8_t selector, uint16_t value);
void fpga_protocol_write_u32_be(uint8_t selector, uint32_t value);

uint8_t fpga_protocol_read_u8(uint8_t selector);
uint16_t fpga_protocol_read_u16_be(uint8_t selector);
uint32_t fpga_protocol_read_u32_be(uint8_t selector);

#endif
