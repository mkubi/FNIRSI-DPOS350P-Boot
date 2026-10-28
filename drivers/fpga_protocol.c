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

    result->final_status = 0U;
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


#define FPGA_TRANSACTION_TIMING_TABLE \
    ((volatile uint32_t *)(uintptr_t)0x42E67558U)
#define FPGA_TRANSACTION_POLL64_LIMIT 100000U
#define FPGA_TRANSACTION_POLL65_LIMIT 500000U
#define FPGA_TRANSACTION_OUTER_ATTEMPTS 4U
#define FPGA_TRANSACTION_TIMING_MAX 3000U
#define FPGA_TRANSACTION_TIMING_STEP 100U

static int fpga_protocol_poll_bit0(
    uint8_t selector,
    uint32_t limit,
    uint32_t *count,
    uint8_t *final)
{
    uint32_t i;
    uint8_t status = 0U;

    for (i = 0U; i < limit; ++i) {
        status = fpga_protocol_read_u8(selector);
        if ((status & 1U) != 0U) {
            *count += i + 1U;
            *final = status;
            return 1;
        }
    }

    *count += limit;
    *final = status;
    return 0;
}

int fpga_protocol_parameter_transaction(
    uint8_t command,
    uint32_t value,
    struct fpga_protocol_transaction_result *result)
{
    volatile uint32_t *timing_table = FPGA_TRANSACTION_TIMING_TABLE;
    struct fpga_protocol_transaction_result local;
    struct fpga_protocol_transaction_result *out = result ? result : &local;
    uint32_t outer;
    uint32_t i;
    uint32_t *words = (uint32_t *)(void *)out;

    for (i = 0U; i < sizeof(*out) / sizeof(*words); ++i)
        words[i] = 0U;

    for (outer = 0U; outer < FPGA_TRANSACTION_OUTER_ATTEMPTS; ++outer) {
        for (;;) {
            uint32_t timing = timing_table[command] & 0xFFFFU;

            out->attempts += 1U;
            out->timing = (uint16_t)timing;

            fpga_protocol_write_u16_be(0x69U, (uint16_t)timing);
            fpga_protocol_write_u8(0x6AU, command);
            fpga_protocol_write_u32_be(0x6BU, value);
            fpga_protocol_write_u8(0x68U, 1U);

            if (!fpga_protocol_poll_bit0(
                    0x64U,
                    FPGA_TRANSACTION_POLL64_LIMIT,
                    &out->poll64_count,
                    &out->status64)) {
                out->error = -1;
            } else {
                fpga_protocol_write_u8(0x68U, 0U);

                if (!fpga_protocol_poll_bit0(
                        0x65U,
                        FPGA_TRANSACTION_POLL65_LIMIT,
                        &out->poll65_count,
                        &out->status65)) {
                    out->error = -2;
                } else {
                    out->echo68 = fpga_protocol_read_u8(0x68U);
                    out->status67 = fpga_protocol_read_u8(0x67U);

                    if (out->echo68 == command &&
                        (out->status67 & 1U) != 0U) {
                        out->result66 =
                            fpga_protocol_read_u32_be(0x66U);
                        out->success = 1;
                        out->error = 0;
                        return 1;
                    }

                    out->error = out->echo68 != command ? -3 : -4;
                }
            }

            timing = timing_table[command] +
                     FPGA_TRANSACTION_TIMING_STEP;
            timing_table[command] = timing;
            if (timing <= FPGA_TRANSACTION_TIMING_MAX)
                continue;

            timing_table[command] = FPGA_TRANSACTION_TIMING_MAX;
            break;
        }
    }

    return 0;
}
