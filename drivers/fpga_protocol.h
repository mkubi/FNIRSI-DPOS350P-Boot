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

struct fpga_protocol_ready_result {
    uint8_t initial_status;
    uint8_t final_status;
    uint32_t polls;
    int ready;
};

void fpga_protocol_acq_prepare(uint8_t mode_byte);
void fpga_protocol_acq_wait_ready(
    uint32_t max_polls,
    struct fpga_protocol_ready_result *result);
void fpga_protocol_stream_set_quantity(uint16_t quantity);
void fpga_protocol_capture_prepare_position(uint16_t position);
void fpga_protocol_capture_select_bank(uint8_t bank);

#endif
