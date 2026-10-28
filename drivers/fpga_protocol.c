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
