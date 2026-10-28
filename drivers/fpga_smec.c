#include "fpga_smec.h"
#include "fpga_bus.h"
#include "fpga_protocol.h"

#define SMEC_POLL64_LIMIT 100000U
#define SMEC_POLL65_LIMIT 500000U
#define SMEC_OUTER_ATTEMPTS 4U

static int poll_bit0(uint8_t selector, uint32_t limit,
                     uint32_t *count, uint8_t *final)
{
    uint32_t i;
    uint8_t value = 0U;

    for (i = 0U; i < limit; ++i) {
        value = fpga_protocol_read_u8(selector);
        if ((value & 1U) != 0U) {
            *count += i + 1U;
            *final = value;
            return 1;
        }
    }
    *count += limit;
    *final = value;
    return 0;
}

static int submit_job(uint8_t index, uint32_t value,
                      struct fpga_smec_init_result *result)
{
    uint32_t outer;
    uint16_t timing;
    uint8_t returned_index;
    uint8_t valid;

    for (outer = 0U; outer < SMEC_OUTER_ATTEMPTS; ++outer) {
        for (timing = 2000U; timing <= 3000U; timing += 100U) {
            result->attempts += 1U;
            result->timing = timing;
            result->current_job = index;

            fpga_protocol_write_u16_be(0x69U, timing);
            fpga_protocol_write_u8(0x6AU, index);
            fpga_protocol_write_u32_be(0x6BU, value);
            fpga_protocol_write_u8(0x68U, 1U);

            if (!poll_bit0(0x64U, SMEC_POLL64_LIMIT,
                           &result->poll64_count, &result->status64)) {
                result->error = -1;
                continue;
            }

            fpga_protocol_write_u8(0x68U, 0U);
            if (!poll_bit0(0x65U, SMEC_POLL65_LIMIT,
                           &result->poll65_count, &result->final65)) {
                result->error = -2;
                continue;
            }

            returned_index = fpga_protocol_read_u8(0x68U);
            valid = fpga_protocol_read_u8(0x67U);
            result->echo68 = returned_index;
            result->status67 = valid;
            if (returned_index == index && (valid & 1U) != 0U) {
                result->result66 = fpga_protocol_read_u32_be(0x66U);
                result->error = 0;
                return 1;
            }
            result->error = returned_index != index ? -3 : -4;
        }
    }
    return 0;
}

int fpga_smec_init(struct fpga_smec_init_result *result)
{
    static const uint32_t values[7] = {
        20U, 64U, 64U, 1000U, 100U, 950U, 200U
    };
    uint32_t i;
    uint32_t *words;

    if (!result)
        return 0;

    words = (uint32_t *)(void *)result;
    for (i = 0U; i < sizeof(*result) / sizeof(*words); ++i)
        words[i] = 0U;

    fpga_bus_control_idle();
    fpga_protocol_write_u8(0x67U, 0x08U);

    for (i = 0U; i < 7U; ++i) {
        if (!submit_job((uint8_t)(i + 1U), values[i], result)) {
            fpga_bus_safe_idle();
            return 0;
        }
        result->completed_jobs = i + 1U;
    }

    result->success = 1;
    fpga_bus_safe_idle();
    return 1;
}
