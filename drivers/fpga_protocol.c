#include "fpga_protocol.h"
#include "fpga_bus.h"

void fpga_protocol_write_u8(uint8_t selector, uint8_t value)
{
    fpga_bus_select(selector);
    fpga_bus_write(value);
}

void fpga_protocol_write_u16_be(uint8_t selector, uint16_t value)
{
    fpga_bus_select(selector);
    fpga_bus_write((uint8_t)(value >> 8));
    fpga_bus_write((uint8_t)value);
}

void fpga_protocol_write_u32_be(uint8_t selector, uint32_t value)
{
    fpga_bus_select(selector);
    fpga_bus_write((uint8_t)(value >> 24));
    fpga_bus_write((uint8_t)(value >> 16));
    fpga_bus_write((uint8_t)(value >> 8));
    fpga_bus_write((uint8_t)value);
}

uint8_t fpga_protocol_read_u8(uint8_t selector)
{
    fpga_bus_select(selector);
    return fpga_bus_read();
}

uint16_t fpga_protocol_read_u16_be(uint8_t selector)
{
    uint16_t value;

    fpga_bus_select(selector);
    value = (uint16_t)fpga_bus_read() << 8;
    value |= fpga_bus_read();
    return value;
}

uint32_t fpga_protocol_read_u32_be(uint8_t selector)
{
    uint32_t value;

    fpga_bus_select(selector);
    value = fpga_bus_read();
    value = (value << 8) | fpga_bus_read();
    value = (value << 8) | fpga_bus_read();
    value = (value << 8) | fpga_bus_read();
    return value;
}

void fpga_protocol_acq_prepare(uint8_t mode_byte)
{
    fpga_protocol_write_u8(0x0FU, 0x00U);
    fpga_protocol_write_u8(0x01U, 0x01U);
    fpga_protocol_write_u8(0x01U, 0x00U);
    fpga_protocol_write_u8(0x11U, 0x00U);
    fpga_protocol_write_u8(0x11U, 0x01U);
    fpga_protocol_write_u8(0x02U, mode_byte);
    fpga_protocol_write_u8(0x0FU, 0x01U);
}

void fpga_protocol_acq_wait_ready(
    uint32_t max_polls,
    struct fpga_protocol_ready_result *result)
{
    uint32_t i;
    uint8_t status;

    result->initial_status = fpga_protocol_read_u8(0x0AU);
    result->final_status = result->initial_status;
    result->polls = 0U;
    result->ready = 0;

    for (i = 0U; i < max_polls; ++i) {
        status = fpga_protocol_read_u8(0x0AU);
        result->final_status = status;
        result->polls = i + 1U;
        if ((status & 1U) != 0U) {
            result->ready = 1;
            break;
        }
    }
}

void fpga_protocol_stream_set_quantity(uint16_t quantity)
{
    fpga_protocol_write_u16_be(0x22U, quantity);
}

void fpga_protocol_capture_prepare_position(uint16_t position)
{
    fpga_protocol_write_u16_be(0x20U, position);
    fpga_protocol_write_u8(0x21U, 0x00U);
    fpga_protocol_write_u8(0x21U, 0x01U);
}

void fpga_protocol_capture_select_bank(uint8_t bank)
{
    fpga_protocol_write_u8(0x1FU, bank);
}
