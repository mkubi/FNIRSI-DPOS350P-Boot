#include <stdint.h>
#include "stock_primitive_smec_test.h"

#ifndef DPOSLAB_PRIMITIVE_TEST
#define DPOSLAB_PRIMITIVE_TEST 2
#endif

#if DPOSLAB_PRIMITIVE_TEST < 2 || DPOSLAB_PRIMITIVE_TEST > 4
#error "DPOSLAB_PRIMITIVE_TEST must be 2, 3, or 4"
#endif

#define STOCK_FPGA_READ_BYTE_ADDR       0x400491B8U
#define STOCK_FPGA_WRITE_PAYLOAD_ADDR   0x40049214U
#define STOCK_FPGA_SELECT_COMMAND_ADDR  0x4004927CU

#define SMEC_POLL64_LIMIT 100000U
#define SMEC_POLL65_LIMIT 750000U

#define STOCK_READ_BYTE \
    ((uint8_t (*)(void))(uintptr_t)STOCK_FPGA_READ_BYTE_ADDR)
#define STOCK_WRITE_PAYLOAD \
    ((void (*)(uint32_t))(uintptr_t)STOCK_FPGA_WRITE_PAYLOAD_ADDR)
#define STOCK_SELECT_COMMAND \
    ((void (*)(uint32_t))(uintptr_t)STOCK_FPGA_SELECT_COMMAND_ADDR)

volatile uint32_t dposlab_primitive_test_mode
    __attribute__((section(".data"))) = DPOSLAB_PRIMITIVE_TEST;

static void trace_reset(struct fpga_smec_poll_trace *trace)
{
    uint32_t i;

    trace->count = 0U;
    trace->changes = 0U;
    trace->bit0_count = 0U;
    trace->first = 0U;
    trace->last = 0U;
    trace->or_value = 0U;
    trace->value_count = 0U;

    for (i = 0U; i < FPGA_SMEC_TRACE_VALUES; ++i)
        trace->values[i] = 0U;
}

static void selected_select(uint8_t selector)
{
#if DPOSLAB_PRIMITIVE_TEST == 2 || DPOSLAB_PRIMITIVE_TEST == 3
    /*
     * A2: fixed Custom selector + stock write/read.
     * A3: fixed Custom selector + Custom write + stock read.
     */
    fpga_test_custom_select_command(selector);
#else
    /* A4 keeps stock selector while isolating Custom read. */
    STOCK_SELECT_COMMAND((uint32_t)selector);
#endif
}

static void selected_write(uint8_t value)
{
#if DPOSLAB_PRIMITIVE_TEST == 3
    fpga_test_custom_write_payload(value);       /* A3: Custom selector + write */
#else
    STOCK_WRITE_PAYLOAD((uint32_t)value);        /* A2/A4: Stock write */
#endif
}

static uint8_t selected_read(void)
{
#if DPOSLAB_PRIMITIVE_TEST == 4
    return fpga_test_custom_read_byte();         /* A4: Custom read */
#else
    return STOCK_READ_BYTE();                    /* A2/A3: Stock read */
#endif
}

static uint8_t read_selector(uint8_t selector)
{
    selected_select(selector);
    return selected_read();
}

static void write_u16_be(uint8_t selector, uint16_t value)
{
    selected_select(selector);
    selected_write((uint8_t)(value >> 8));
    selected_write((uint8_t)value);
}

static void write_u32_be(uint8_t selector, uint32_t value)
{
    selected_select(selector);
    selected_write((uint8_t)(value >> 24));
    selected_write((uint8_t)(value >> 16));
    selected_write((uint8_t)(value >> 8));
    selected_write((uint8_t)value);
}

static uint32_t read_u32_be(uint8_t selector)
{
    uint32_t value;

    selected_select(selector);
    value = selected_read();
    value = (value << 8) | selected_read();
    value = (value << 8) | selected_read();
    value = (value << 8) | selected_read();
    return value;
}

static int poll_bit0(uint8_t selector,
                     uint32_t limit,
                     struct fpga_smec_poll_trace *trace)
{
    uint32_t i;
    uint8_t value;
    uint8_t previous = 0U;

    trace_reset(trace);

    for (i = 0U; i < limit; ++i) {
        value = read_selector(selector);

        if (i == 0U) {
            trace->first = value;
            trace->values[0] = value;
            trace->value_count = 1U;
        } else if (value != previous) {
            trace->changes += 1U;
            if (trace->value_count < FPGA_SMEC_TRACE_VALUES)
                trace->values[trace->value_count++] = value;
        }

        trace->count = i + 1U;
        trace->last = value;
        trace->or_value |= value;

        if ((value & 1U) != 0U) {
            trace->bit0_count += 1U;
            return 1;
        }

        previous = value;
    }

    return 0;
}

static int transaction_once(uint8_t index,
                            uint32_t value,
                            uint16_t timing,
                            struct fpga_smec_job_trace *trace)
{
    trace->index = index;
    trace->echo68 = 0U;
    trace->status67 = 0U;
    trace->completed = 0U;
    trace->timing = timing;
    trace->reserved = 0U;
    trace->value = value;
    trace->result66 = 0U;
    trace->error = 0;
    trace_reset(&trace->poll64);
    trace_reset(&trace->poll65);

    write_u16_be(0x69U, timing);
    selected_select(0x6AU);
    selected_write(index);
    write_u32_be(0x6BU, value);

    selected_select(0x68U);
    selected_write(1U);

    if (!poll_bit0(0x64U, SMEC_POLL64_LIMIT, &trace->poll64)) {
        trace->error = -1;
        return 0;
    }

    selected_select(0x68U);
    selected_write(0U);

    if (!poll_bit0(0x65U, SMEC_POLL65_LIMIT, &trace->poll65)) {
        trace->error = -2;
        return 0;
    }

    trace->echo68 = read_selector(0x68U);
    trace->status67 = read_selector(0x67U);

    if (trace->echo68 != index) {
        trace->error = -3;
        return 0;
    }

    if ((trace->status67 & 1U) == 0U) {
        trace->error = -4;
        return 0;
    }

    trace->result66 = read_u32_be(0x66U);
    trace->completed = 1U;
    return 1;
}

static int transaction_retry(uint8_t index,
                             uint32_t value,
                             struct fpga_smec_job_trace *trace,
                             uint32_t *attempts,
                             uint32_t *final_timing,
                             uint32_t *final_outer)
{
    uint32_t outer;
    uint32_t timing = 2000U;

    *attempts = 0U;
    *final_timing = timing;
    *final_outer = 0U;

    for (outer = 0U; outer < 4U; ++outer) {
        do {
            *attempts += 1U;
            *final_timing = timing;
            *final_outer = outer;

            if (transaction_once(index, value, (uint16_t)timing, trace))
                return 1;

            timing += 100U;
        } while (timing <= 3000U);

        timing = 3000U;
    }

    return 0;
}

void dposlab_custom_smec_stock_primitives_test(
    struct fpga_smec_stage24_pe_result *result)
{
    uint32_t *words;
    uint32_t i;

    if (!result)
        return;

    words = (uint32_t *)(void *)result;
    for (i = 0U; i < sizeof(*result) / sizeof(*words); ++i)
        words[i] = 0U;

    selected_select(0x67U);
    selected_write(0x08U);
    write_u16_be(0x69U, 2000U);

    (void)transaction_retry(
        1U, 20U, &result->job1,
        &result->job1_attempts,
        &result->job1_final_timing,
        &result->job1_outer_attempt);

    if (result->job1.completed != 0U) {
        (void)transaction_retry(
            2U, 64U, &result->job2,
            &result->job2_attempts,
            &result->job2_final_timing,
            &result->job2_outer_attempt);
    }
}
