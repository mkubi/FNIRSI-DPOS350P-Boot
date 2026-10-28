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

struct fpga_protocol_transaction_result {
    uint32_t result66;
    uint32_t attempts;
    uint32_t poll64_count;
    uint32_t poll65_count;
    uint16_t timing;
    uint8_t echo68;
    uint8_t status64;
    uint8_t status65;
    uint8_t status67;
    int success;
    int error;
};

/* Bounded custom replacement for stock transaction engine 0x4004A0B0. */
int fpga_protocol_parameter_transaction(
    uint8_t command,
    uint32_t value,
    struct fpga_protocol_transaction_result *result);

#endif
