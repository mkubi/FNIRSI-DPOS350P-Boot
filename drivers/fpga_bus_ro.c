#include "fpga_bus_ro.h"

#define PIO_BASE       0x01C20800u
#define PE_CFG0        (*(volatile uint32_t *)(PIO_BASE + 0x90u))
#define PE_CFG1        (*(volatile uint32_t *)(PIO_BASE + 0x94u))
#define PE_DATA        (*(volatile uint32_t *)(PIO_BASE + 0xA0u))
#define FPGA_BUS_DIRECTION_STATE     (*(volatile uint8_t *)0x42E68D18U)

#define FPGA_MAGIC     0xA5A55A5Au
#define PE_CTRL_MASK   0x0000000Fu
#define PE_DATA_MASK   0x0000FF00u

static inline void barrier(void)
{
    __asm__ volatile ("dsb sy" ::: "memory");
}

static inline void pe_set(unsigned pin)
{
    PE_DATA |= 1u << pin;
    barrier();
}

static inline void pe_clear(unsigned pin)
{
    PE_DATA &= ~(1u << pin);
    barrier();
}

static void data_bus_input(void)
{
    /*
     * Exact stock direction bookkeeping:
     * state 0 means PE8..PE15 are GPIO inputs.
     */
    FPGA_BUS_DIRECTION_STATE = 0U;
    PE_CFG1 = 0x00000000U;
    barrier();
}

static void data_bus_output(void)
{
    /*
     * Exact stock 0x400490F4 direction bookkeeping:
     * state 1 means PE8..PE15 are GPIO outputs.
     */
    FPGA_BUS_DIRECTION_STATE = 1U;
    PE_CFG1 = 0x11111111U;
    barrier();
}

static void write_data_byte(uint8_t value)
{
    uint32_t data = PE_DATA;
    data &= ~PE_DATA_MASK;
    data |= (uint32_t)value << 8;
    PE_DATA = data;
    barrier();
}

static void control_bus_init(void)
{
    uint32_t cfg = PE_CFG0;

    /* PE0..PE3: GPIO outputs. Preserve PE4..PE7. */
    cfg &= ~0x0000FFFFu;
    cfg |=  0x00001111u;
    PE_CFG0 = cfg;
    barrier();

    /* EXE2 0x40049110 idle state: PE0 high, PE1 low. */
    PE_DATA = (PE_DATA & ~PE_CTRL_MASK) | 0x00000001u;
    barrier();
}


static void fpga_select_baseline(uint8_t value)
{
    data_bus_output();

    /* Previously verified diagnostic selector: PE2=1, PE3=1, PE0 0->1. */
    pe_set(2);
    pe_set(3);
    write_data_byte(value);
    pe_clear(0);
    pe_set(0);
}

static void fpga_select_command(uint8_t value)
{
    data_bus_output();

    /*
     * Exact stock EXE2 0x4004927C selector primitive:
     *   PE2 HIGH
     *   PE3 HIGH
     *   PE8..PE15 = selector
     *   PE0 LOW -> HIGH
     *
     * The primitive returns with PE0, PE2 and PE3 HIGH.
     */
    pe_set(2);
    pe_set(3);
    write_data_byte(value);
    pe_clear(0);
    pe_set(0);
}

static void fpga_write_payload(uint8_t value)
{
    data_bus_output();

    /*
     * Exact stock EXE2 0x40049214 payload-write primitive:
     *   PE2 HIGH
     *   PE3 LOW
     *   PE8..PE15 = payload
     *   PE0 LOW -> HIGH
     *
     * The primitive returns with PE0 and PE2 HIGH, PE3 LOW.
     */
    pe_set(2);
    pe_clear(3);
    write_data_byte(value);
    pe_clear(0);
    pe_set(0);
}

static void fpga_settle_delay(void)
{
    /* Deliberately short bus-settle delay. This is not a millisecond delay.
       The volatile loop prevents -O2 from removing the timing gap. */
    volatile unsigned i;
    for (i = 0; i < 64U; ++i)
        __asm__ volatile ("nop");
    barrier();
}

static uint8_t fpga_read_byte(void)
{
    uint8_t value;

    data_bus_input();

    /*
     * Exact stock EXE2 0x400491B8 read primitive:
     *   PE2 LOW
     *   PE3 LOW
     *   PE0 LOW -> HIGH
     *   sample PE8..PE15 after the rising edge
     */
    pe_clear(2);
    pe_clear(3);
    pe_clear(0);
    pe_set(0);
    value = (uint8_t)((PE_DATA >> 8) & 0xFFu);

    return value;
}

void fpga_bus_safe_idle(void)
{
    data_bus_input();
    control_bus_init();
}

void fpga_test_custom_select_command(uint8_t selector)
{
    fpga_select_command(selector);
}

void fpga_test_custom_write_payload(uint8_t value)
{
    fpga_write_payload(value);
}

uint8_t fpga_test_custom_read_byte(void)
{
    return fpga_read_byte();
}

void fpga_diag_read_id(struct fpga_diag_result *result)
{
    unsigned i;
    uint32_t word = 0;
    uint32_t shifted = 0;

    result->pe_cfg0_before = PE_CFG0;
    result->pe_cfg1_before = PE_CFG1;
    result->pe_data_before = PE_DATA;

    control_bus_init();
    fpga_select_command(1u);
    fpga_settle_delay();

    /* Read one extra byte. If the first cycle is a dummy/stale byte, the
       expected magic will appear in raw[1]..raw[4]. */
    for (i = 0; i < 5; ++i)
        result->raw[i] = fpga_read_byte();

    for (i = 0; i < 4; ++i)
        word = (word << 8) | result->raw[i];
    for (i = 1; i < 5; ++i)
        shifted = (shifted << 8) | result->raw[i];

    result->id_word = word;
    result->id_word_shifted = shifted;
    result->present = (word == FPGA_MAGIC) || (shifted == FPGA_MAGIC);

    /* Restore the original bus-idle convention: data input, PE0 high,
       PE1 low. PE2/PE3 are deasserted high after the transaction. */
    data_bus_input();
    pe_set(0);
    pe_clear(1);
    pe_set(2);
    pe_set(3);

    result->pe_cfg0_after = PE_CFG0;
    result->pe_cfg1_after = PE_CFG1;
    result->pe_data_after = PE_DATA;
}


void fpga_capture_arm_probe(struct fpga_capture_arm_probe_result *result)
{
    result->pe_cfg0_before = PE_CFG0;
    result->pe_cfg1_before = PE_CFG1;
    result->pe_data_before = PE_DATA;

    control_bus_init();

    /* Confirmed EXE2 0x4003F8E0 sequence. */
    fpga_select_command(0x08U);
    fpga_settle_delay();
    fpga_write_payload(0x00U);
    fpga_settle_delay();
    fpga_select_command(0x08U);
    fpga_settle_delay();
    fpga_write_payload(0x01U);
    fpga_settle_delay();

    /* Leave the GPIO bus in the same safe idle convention as ID probe. */
    fpga_bus_safe_idle();

    result->pe_cfg0_after = PE_CFG0;
    result->pe_cfg1_after = PE_CFG1;
    result->pe_data_after = PE_DATA;
}


void fpga_select_bank0(void)
{
    control_bus_init();
    fpga_select_command(0x15U);
    fpga_settle_delay();
    fpga_write_payload(0x00U);
    fpga_settle_delay();
    fpga_bus_safe_idle();
}



void fpga_set_stream_quantity(uint16_t quantity)
{
    control_bus_init();
    /* Exact tail of stock 0x400405E0: FPGA selector 0x22, big-endian u16. */
    fpga_select_command(0x22U);
    fpga_settle_delay();
    fpga_write_payload((uint8_t)(quantity >> 8));
    fpga_settle_delay();
    fpga_write_payload((uint8_t)quantity);
    fpga_settle_delay();
}

void fpga_prepare_csi_bank0_position(uint16_t position)
{
    control_bus_init();

    /* Exact 0x4003F988 encoding: selector 0x20, big-endian u16,
     * then selector 0x21 with a 0 -> 1 apply pulse. */
    fpga_select_command(0x20U);
    fpga_settle_delay();
    fpga_write_payload((uint8_t)(position >> 8));
    fpga_settle_delay();
    fpga_write_payload((uint8_t)position);
    fpga_settle_delay();
    fpga_select_command(0x21U);
    fpga_settle_delay();
    fpga_write_payload(0x00U);
    fpga_settle_delay();
    fpga_select_command(0x21U);
    fpga_settle_delay();
    fpga_write_payload(0x01U);
    fpga_settle_delay();

    /* Exact per-capture selector from 0x4003FA94: FPGA 0x1F <- bank 0. */
    fpga_select_command(0x1FU);
    fpga_settle_delay();
    fpga_write_payload(0x00U);
    fpga_settle_delay();
}

void fpga_prepare_csi_bank0(void)
{
    fpga_prepare_csi_bank0_position(0U);
}

void fpga_bus_enter_csi_mode(void)
{
    uint32_t cfg = PE_CFG0;

    /* Stock 0x40065EF8: PE0, PE2, PE3 and PE8..PE15 -> function 2. */
    cfg &= ~((0xFU << 0) | (0xFU << 8) | (0xFU << 12));
    cfg |=  ((2U << 0) | (2U << 8) | (2U << 12));
    PE_CFG0 = cfg;
    PE_CFG1 = 0x22222222U;
    barrier();
}

void fpga_bus_leave_csi_mode(void)
{
    uint32_t cfg = PE_CFG0;

    /* Stock 0x40049110: PE0..PE3 back to GPIO, PE0 high, PE1 low. */
    cfg &= ~0x0000FFFFU;
    PE_CFG0 = cfg;
    pe_set(0U);
    pe_clear(1U);
    data_bus_input();
    barrier();
}

void fpga_capture_gate(int high)
{
    /* PE1 remains GPIO while the sample pins belong to CSI. */
    if (high)
        pe_set(1U);
    else
        pe_clear(1U);
}

void fpga_acq_ready_probe(uint8_t mode_byte, uint32_t max_polls,
                          struct fpga_acq_ready_result *result)
{
    uint32_t polls;
    uint8_t status = 0U;

    result->polls = 0U;
    result->final_status = 0U;
    result->mode_byte = mode_byte;
    result->ready = 0;

    control_bus_init();

    /* Stock higher-level selector for the first acquisition block/lane. */
    fpga_select_command(0x15U);
    fpga_settle_delay();
    fpga_write_payload(0x00U);
    fpga_settle_delay();

    /* Exact 0x4003FA10 acquisition prepare/start sequence. */
    fpga_select_command(0x0FU);
    fpga_write_payload(0x00U);
    fpga_select_command(0x01U);
    fpga_write_payload(0x01U);
    fpga_select_command(0x01U);
    fpga_write_payload(0x00U);
    fpga_select_command(0x11U);
    fpga_write_payload(0x00U);
    fpga_select_command(0x11U);
    fpga_write_payload(0x01U);
    fpga_select_command(0x02U);
    fpga_write_payload(mode_byte);
    fpga_select_command(0x0FU);
    fpga_write_payload(0x01U);
    fpga_settle_delay();

    /* Exact readiness source used by 0x4003FCC0: FPGA 0x0A bit 0. */
    for (polls = 0U; polls < max_polls; ++polls) {
        fpga_select_command(0x0AU);
        status = fpga_read_byte();
        if ((status & 1U) != 0U) {
            result->polls = polls + 1U;
            result->final_status = status;
            result->ready = 1;
            fpga_bus_safe_idle();
            return;
        }
    }

    result->polls = max_polls;
    result->final_status = status;
    fpga_bus_safe_idle();
}

void fpga_acq_position_probe(uint8_t mode_byte, uint32_t max_polls,
                             struct fpga_acq_position_result *result)
{
    uint8_t high;
    uint8_t low;

    result->valid_status = 0U;
    result->position = 0U;
    result->valid = 0;

    fpga_acq_ready_probe(mode_byte, max_polls, &result->ready);
    if (!result->ready.ready)
        return;

    control_bus_init();

    /* Exact stock 0x40063770 validity read. */
    fpga_select_command(0x18U);
    result->valid_status = fpga_read_byte();
    if ((result->valid_status & 1U) == 0U) {
        fpga_bus_safe_idle();
        return;
    }

    /* Exact stock 0x400637B4 path: selector 0x14, then big-endian u16. */
    fpga_select_command(0x14U);
    high = fpga_read_byte();
    low = fpga_read_byte();
    result->position = (uint16_t)(((uint16_t)high << 8) | low);
    result->valid = 1;
    fpga_bus_safe_idle();
}

void fpga_stream_activity_probe(uint32_t samples,
                                struct fpga_stream_activity_result *result)
{
    const uint32_t sample_mask = (1U << 0) | (1U << 2) | (1U << 3) |
                                 PE_DATA_MASK;
    uint32_t cfg;
    uint32_t previous;
    uint32_t current;
    uint32_t i;

    result->first = 0U;
    result->last = 0U;
    result->changed_mask = 0U;
    result->high_mask = 0U;
    result->low_mask = sample_mask;
    result->samples = samples;

    /* PE0/2/3 and PE8..15 become GPIO inputs. PE1 remains GPIO output
     * because stock uses PE1 as the active-high stream gate. */
    cfg = PE_CFG0;
    cfg &= ~0x0000FFFFU;
    cfg |= (1U << 4); /* PE1 function 1 = GPIO output. */
    PE_CFG0 = cfg;
    PE_CFG1 = 0x00000000U;
    barrier();

    pe_set(1U);
    previous = PE_DATA & sample_mask;
    result->first = previous;
    result->high_mask = previous;
    result->low_mask = sample_mask & ~previous;

    for (i = 0U; i < samples; ++i) {
        current = PE_DATA & sample_mask;
        result->changed_mask |= previous ^ current;
        result->high_mask |= current;
        result->low_mask |= sample_mask & ~current;
        previous = current;
    }

    result->last = previous;
    pe_clear(1U);
    fpga_bus_leave_csi_mode();
}

void fpga_diag_read_id_stock_raw4(struct fpga_diag_result *result)
{
    unsigned i;
    uint32_t word = 0U;

    result->pe_cfg0_before = PE_CFG0;
    result->pe_cfg1_before = PE_CFG1;
    result->pe_data_before = PE_DATA;
    for (i = 0U; i < 5U; ++i)
        result->raw[i] = 0U;

    control_bus_init();
    fpga_select_baseline(0x01U);

    /* Stock 0x400492E4 calls the read primitive exactly four times. */
    for (i = 0U; i < 4U; ++i) {
        result->raw[i] = fpga_read_byte();
        word = (word << 8) | result->raw[i];
    }

    result->id_word = word;
    result->id_word_shifted = 0U;
    result->present = word == FPGA_MAGIC;

    data_bus_input();
    pe_set(0U);
    pe_clear(1U);
    pe_set(2U);
    pe_set(3U);

    result->pe_cfg0_after = PE_CFG0;
    result->pe_cfg1_after = PE_CFG1;
    result->pe_data_after = PE_DATA;
}

void fpga_diag_read_id_baseline(struct fpga_diag_result *result)
{
    unsigned i;
    uint32_t word = 0U;
    uint32_t shifted = 0U;

    result->pe_cfg0_before = PE_CFG0;
    result->pe_cfg1_before = PE_CFG1;
    result->pe_data_before = PE_DATA;

    control_bus_init();
    fpga_select_baseline(0x01U);
    fpga_settle_delay();

    for (i = 0U; i < 5U; ++i)
        result->raw[i] = fpga_read_byte();

    for (i = 0U; i < 4U; ++i)
        word = (word << 8) | result->raw[i];
    for (i = 1U; i < 5U; ++i)
        shifted = (shifted << 8) | result->raw[i];

    result->id_word = word;
    result->id_word_shifted = shifted;
    result->present = (word == FPGA_MAGIC) || (shifted == FPGA_MAGIC);

    data_bus_input();
    pe_set(0);
    pe_clear(1);
    pe_set(2);
    pe_set(3);

    result->pe_cfg0_after = PE_CFG0;
    result->pe_cfg1_after = PE_CFG1;
    result->pe_data_after = PE_DATA;
}


#define SMEC_POLL64_LIMIT 100000U
#define SMEC_POLL65_LIMIT 500000U
#define SMEC_OUTER_ATTEMPTS 4U

static void fpga_write_u16_be(uint8_t selector, uint16_t value)
{
    fpga_select_command(selector);
    fpga_write_payload((uint8_t)(value >> 8));
    fpga_write_payload((uint8_t)value);
}

static void fpga_write_u32_be(uint8_t selector, uint32_t value)
{
    fpga_select_command(selector);
    fpga_write_payload((uint8_t)(value >> 24));
    fpga_write_payload((uint8_t)(value >> 16));
    fpga_write_payload((uint8_t)(value >> 8));
    fpga_write_payload((uint8_t)value);
}

static uint8_t fpga_read_selector_u8(uint8_t selector)
{
    fpga_select_command(selector);
    return fpga_read_byte();
}

static uint32_t fpga_read_selector_u32_be(uint8_t selector)
{
    uint32_t value;
    fpga_select_command(selector);
    value = fpga_read_byte();
    value = (value << 8) | fpga_read_byte();
    value = (value << 8) | fpga_read_byte();
    value = (value << 8) | fpga_read_byte();
    return value;
}

static int smec_poll_bit0(uint8_t selector, uint32_t limit,
                          uint32_t *count, uint8_t *final)
{
    uint32_t i;
    uint8_t value = 0U;
    for (i = 0U; i < limit; ++i) {
        value = fpga_read_selector_u8(selector);
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

static int smec_submit_job(uint8_t index, uint32_t value,
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

            fpga_write_u16_be(0x69U, timing);
            fpga_select_command(0x6AU);
            fpga_write_payload(index);
            fpga_write_u32_be(0x6BU, value);

            fpga_select_command(0x68U);
            fpga_write_payload(1U);
            if (!smec_poll_bit0(0x64U, SMEC_POLL64_LIMIT,
                                &result->poll64_count,
                                &result->status64)) {
                result->error = -1;
                continue;
            }

            fpga_select_command(0x68U);
            fpga_write_payload(0U);
            if (!smec_poll_bit0(0x65U, SMEC_POLL65_LIMIT,
                                &result->poll65_count,
                                &result->final65)) {
                result->error = -2;
                continue;
            }

            returned_index = fpga_read_selector_u8(0x68U);
            valid = fpga_read_selector_u8(0x67U);
            result->echo68 = returned_index;
            result->status67 = valid;
            if (returned_index == index && (valid & 1U) != 0U) {
                result->result66 = fpga_read_selector_u32_be(0x66U);
                result->error = 0;
                return 1;
            }
            result->error = returned_index != index ? -3 : -4;
        }
    }
    return 0;
}

static void control_bus_stock_idle_cleanup(void);

int fpga_smec_init_baseline(struct fpga_smec_init_result *result)
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

    control_bus_stock_idle_cleanup();
    fpga_select_command(0x67U);
    fpga_write_payload(0x08U);

    for (i = 0U; i < 7U; ++i) {
        if (!smec_submit_job((uint8_t)(i + 1U), values[i], result)) {
            fpga_bus_safe_idle();
            return 0;
        }
        result->completed_jobs = i + 1U;
    }

    result->success = 1;
    fpga_bus_safe_idle();
    return 1;
}


#define SMEC_INVESTIGATE_POLL64 100000U
#define SMEC_INVESTIGATE_POLL65 750000U

static void smec_trace_reset(struct fpga_smec_poll_trace *trace)
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

static int smec_poll_trace(uint8_t selector, uint32_t limit,
                           struct fpga_smec_poll_trace *trace)
{
    uint32_t i;
    uint8_t value;
    uint8_t previous = 0U;

    smec_trace_reset(trace);
    for (i = 0U; i < limit; ++i) {
        value = fpga_read_selector_u8(selector);
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

static int smec_trace_one_job_timing(uint8_t index, uint32_t value,
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
    smec_trace_reset(&trace->poll64);
    smec_trace_reset(&trace->poll65);

    fpga_write_u16_be(0x69U, trace->timing);
    fpga_select_command(0x6AU);
    fpga_write_payload(index);
    fpga_write_u32_be(0x6BU, value);
    fpga_select_command(0x68U);
    fpga_write_payload(1U);

    if (!smec_poll_trace(0x64U, SMEC_INVESTIGATE_POLL64,
                         &trace->poll64)) {
        trace->error = -1;
        return 0;
    }

    fpga_select_command(0x68U);
    fpga_write_payload(0U);
    if (!smec_poll_trace(0x65U, SMEC_INVESTIGATE_POLL65,
                         &trace->poll65)) {
        trace->error = -2;
        return 0;
    }

    trace->echo68 = fpga_read_selector_u8(0x68U);
    trace->status67 = fpga_read_selector_u8(0x67U);
    if (trace->echo68 != index) {
        trace->error = -3;
        return 0;
    }
    if ((trace->status67 & 1U) == 0U) {
        trace->error = -4;
        return 0;
    }
    trace->result66 = fpga_read_selector_u32_be(0x66U);
    trace->completed = 1U;
    return 1;
}

static int smec_trace_one_job(uint8_t index, uint32_t value,
                              struct fpga_smec_job_trace *trace)
{
    return smec_trace_one_job_timing(index, value, 2000U, trace);
}

static int smec_poll_fresh_bit0(
    uint8_t selector,
    uint32_t zero_limit,
    uint32_t ready_limit,
    uint32_t *saw_zero,
    uint32_t *zero_polls,
    uint32_t *ready_polls,
    uint8_t *final_value)
{
    uint32_t i;
    uint8_t value = 0U;

    *saw_zero = 0U;
    *zero_polls = 0U;
    *ready_polls = 0U;
    *final_value = 0U;

    /* Reject a sticky READY from the preceding transaction. */
    for (i = 0U; i < zero_limit; ++i) {
        value = fpga_read_selector_u8(selector);
        *zero_polls = i + 1U;
        if ((value & 1U) == 0U) {
            *saw_zero = 1U;
            break;
        }
    }

    if (*saw_zero == 0U) {
        *final_value = value;
        return 0;
    }

    /* Accept completion only after observing a fresh BUSY -> READY edge. */
    for (i = 0U; i < ready_limit; ++i) {
        value = fpga_read_selector_u8(selector);
        *ready_polls = i + 1U;
        if ((value & 1U) != 0U) {
            *final_value = value;
            return 1;
        }
    }

    *final_value = value;
    return 0;
}

static int __attribute__((unused)) smec_trace_job2_fresh_handshake(
    uint32_t value,
    struct fpga_smec_job_trace *trace,
    struct fpga_smec_stage24_pe_result *result)
{
    uint8_t returned_index;
    uint8_t valid;

    trace->index = 2U;
    trace->echo68 = 0U;
    trace->status67 = 0U;
    trace->completed = 0U;
    trace->timing = 2000U;
    trace->reserved = 0U;
    trace->value = value;
    trace->result66 = 0U;
    trace->error = 0;
    smec_trace_reset(&trace->poll64);
    smec_trace_reset(&trace->poll65);

    fpga_write_u16_be(0x69U, trace->timing);
    fpga_select_command(0x6AU);
    fpga_write_payload(2U);
    fpga_write_u32_be(0x6BU, value);

    fpga_select_command(0x68U);
    fpga_write_payload(1U);

    if (!smec_poll_fresh_bit0(
            0x64U,
            SMEC_INVESTIGATE_POLL64,
            SMEC_INVESTIGATE_POLL64,
            &result->job2_p64_saw_zero,
            &result->job2_p64_zero_polls,
            &result->job2_p64_ready_polls,
            &trace->poll64.last)) {
        trace->error = -1;
        return 0;
    }
    trace->poll64.first = 0U;
    trace->poll64.or_value = trace->poll64.last;
    trace->poll64.count = result->job2_p64_zero_polls +
                          result->job2_p64_ready_polls;

    fpga_select_command(0x68U);
    fpga_write_payload(0U);

    if (!smec_poll_fresh_bit0(
            0x65U,
            SMEC_INVESTIGATE_POLL65,
            SMEC_INVESTIGATE_POLL65,
            &result->job2_p65_saw_zero,
            &result->job2_p65_zero_polls,
            &result->job2_p65_ready_polls,
            &trace->poll65.last)) {
        trace->error = -2;
        return 0;
    }
    trace->poll65.first = 0U;
    trace->poll65.or_value = trace->poll65.last;
    trace->poll65.count = result->job2_p65_zero_polls +
                          result->job2_p65_ready_polls;

    returned_index = fpga_read_selector_u8(0x68U);
    valid = fpga_read_selector_u8(0x67U);
    trace->echo68 = returned_index;
    trace->status67 = valid;

    if (returned_index != 2U) {
        trace->error = -3;
        return 0;
    }
    if ((valid & 1U) == 0U) {
        trace->error = -4;
        return 0;
    }

    trace->result66 = fpga_read_selector_u32_be(0x66U);
    trace->completed = 1U;
    return 1;
}

static int smec_trace_stock_transaction(
    uint8_t index,
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

    /* Exact retry controller reconstructed from stock EXE2 0x4004A0B0. */
    for (outer = 0U; outer < 4U; ++outer) {
        do {
            *attempts += 1U;
            *final_timing = timing;
            *final_outer = outer;

            if (smec_trace_one_job_timing(index, value,
                                          (uint16_t)timing, trace))
                return 1;

            timing += 100U;
        } while (timing <= 3000U);

        /* Stock clamps the per-ID timing to 3000 for the next outer try. */
        timing = 3000U;
    }

    return 0;
}


void fpga_smec_investigate_job2(struct fpga_smec_job2_investigation *result)
{
    uint32_t *words;
    uint32_t i;
    volatile uint32_t delay;

    if (!result)
        return;
    words = (uint32_t *)(void *)result;
    for (i = 0U; i < sizeof(*result) / sizeof(*words); ++i)
        words[i] = 0U;

    control_bus_init();
    fpga_select_command(0x67U);
    fpga_write_payload(0x08U);

    if (smec_trace_one_job(1U, 20U, &result->job1)) {
        /* Fixed bounded inter-job gap. It is recorded as part of this exact
         * Stage 22 experiment and is not claimed to be stock behavior. */
        for (delay = 0U; delay < 24000U; ++delay)
            __asm__ volatile ("nop");
        (void)smec_trace_one_job(2U, 64U, &result->job2);
    }
    fpga_bus_safe_idle();
}


void fpga_smec_debug_job2_recovery(struct fpga_smec_job2_recovery *result)
{
    uint32_t *words;
    uint32_t i;

    if (!result)
        return;
    words = (uint32_t *)(void *)result;
    for (i = 0U; i < sizeof(*result) / sizeof(*words); ++i)
        words[i] = 0U;

    control_bus_init();
    fpga_select_command(0x67U);
    fpga_write_payload(0x08U);

    if (!smec_trace_one_job(1U, 20U, &result->job1)) {
        fpga_bus_safe_idle();
        return;
    }

    /* Passive selector snapshots immediately after the successful job. */
    result->idle64_after_job1 = fpga_read_selector_u8(0x64U);
    result->idle65_after_job1 = fpga_read_selector_u8(0x65U);

    if (smec_trace_one_job(2U, 64U, &result->job2_normal)) {
        fpga_bus_safe_idle();
        return;
    }

    result->idle64_after_fail = fpga_read_selector_u8(0x64U);
    result->idle65_after_fail = fpga_read_selector_u8(0x65U);

    /* Diagnostic recovery hypothesis: repeat the confirmed SMEC_Init re-arm
     * write before resubmitting exactly the same Job 2 transaction. */
    fpga_select_command(0x67U);
    fpga_write_payload(0x08U);
    (void)smec_trace_one_job(2U, 64U, &result->job2_rearmed);
    fpga_bus_safe_idle();
}






#define PE_PULL0 (*(volatile uint32_t *)0x01C208ACU)
#define PE_CFG0_CONTROL_MASK 0x0000FFFFU
#define PE_PULL_CONTROL_MASK 0x000000FFU

static void control_bus_stock_idle_cleanup(void)
{
    uint32_t value;

    /* Stock 0x40049110 configures PE0..PE3 as function 1 and disables
     * their pull resistors. Preserve PE4..PE7 and their pull state. */
    value = PE_CFG0;
    value &= ~PE_CFG0_CONTROL_MASK;
    value |= 0x00001111U;
    PE_CFG0 = value;
    barrier();

    value = PE_PULL0;
    value &= ~PE_PULL_CONTROL_MASK;
    PE_PULL0 = value;
    barrier();

    /* Stock 0x400490DC leaves PE8..PE15 as inputs before the first bus
     * primitive changes direction explicitly. */
    PE_CFG1 = 0x00000000U;
    barrier();

    /* Stock idle levels: PE0 high, PE1 low. Leave PE2/PE3 unchanged, just
     * as the stock helpers do after configuring their function. */
    value = PE_DATA;
    value |= 1U << 0;
    value &= ~(1U << 1);
    PE_DATA = value;
    barrier();
}

void fpga_smec_stage24_pe_cleanup_test(
    struct fpga_smec_stage24_pe_result *result)
{
    uint32_t *words;
    uint32_t i;

    if (!result)
        return;

    words = (uint32_t *)(void *)result;
    for (i = 0U; i < sizeof(*result) / sizeof(*words); ++i)
        words[i] = 0U;

    result->cfg0_before = PE_CFG0;
    result->cfg1_before = PE_CFG1;
    result->pull0_before = PE_PULL0;
    result->data_before = PE_DATA;

    control_bus_stock_idle_cleanup();

    result->cfg0_after = PE_CFG0;
    result->cfg1_after = PE_CFG1;
    result->pull0_after = PE_PULL0;
    result->data_after = PE_DATA;

    fpga_select_command(0x67U);
    fpga_write_payload(0x08U);

    if (!smec_trace_one_job(1U, 20U, &result->job1)) {
        fpga_bus_safe_idle();
        return;
    }

    result->idle64_after_job1 = fpga_read_selector_u8(0x64U);
    result->idle65_after_job1 = fpga_read_selector_u8(0x65U);

    (void)smec_trace_one_job(2U, 64U, &result->job2);
    result->idle64_after_job2 = fpga_read_selector_u8(0x64U);
    result->idle65_after_job2 = fpga_read_selector_u8(0x65U);
    fpga_bus_safe_idle();
}

static void __attribute__((unused)) smec_stage29_delay_2s(void)
{
    /*
     * Diagnostic stabilization window only. The exact wall time depends on
     * the current CPU clock, but the volatile loop prevents optimization and
     * intentionally provides an approximately two-second pause on this build.
     */
    volatile uint32_t outer;
    volatile uint32_t inner;

    for (outer = 0U; outer < 2000U; ++outer) {
        for (inner = 0U; inner < 24000U; ++inner)
            __asm__ volatile ("nop");
    }
}

#define STOCK_PART2_CCU_REG  (*(volatile uint32_t *)0x01C20144U)
#define STOCK_PART2_DIV_REG  (*(volatile uint32_t *)0x01C20C8CU)

static void __attribute__((unused)) platform_system_init_part2(
    struct fpga_smec_stage24_pe_result *result)
{
    uint32_t value;

    result->part2_ccu_before = STOCK_PART2_CCU_REG;
    result->part2_div_before = STOCK_PART2_DIV_REG;

    /*
     * Stock 0x40042D54:
     * set bit 31 at 0x01C20144.
     */
    value = STOCK_PART2_CCU_REG;
    STOCK_PART2_CCU_REG = value | 0x80000000U;

    /*
     * Stock 0x40042D40:
     * channel 0 divider = 24000 - 1.
     */
    value = STOCK_PART2_DIV_REG;
    value = (value & 0xFFFF0000U) | 0x00005DBFU;
    STOCK_PART2_DIV_REG = value;

    /*
     * Stock 0x40042D4C:
     * channel 1 divider = 24 - 1.
     */
    value = STOCK_PART2_DIV_REG;
    value = (value & 0x0000FFFFU) | 0x00170000U;
    STOCK_PART2_DIV_REG = value;

    result->part2_ccu_after = STOCK_PART2_CCU_REG;
    result->part2_div_after = STOCK_PART2_DIV_REG;

    result->part2_ok =
        ((result->part2_ccu_after & 0x80000000U) != 0U) &&
        ((result->part2_div_after & 0x0000FFFFU) ==
         0x00005DBFU) &&
        ((result->part2_div_after & 0xFFFF0000U) ==
         0x00170000U);
}

void fpga_smec_custom_job12_test(
    struct fpga_smec_stage24_pe_result *result)
{
    uint32_t *words;
    uint32_t i;

    if (!result)
        return;

    words = (uint32_t *)(void *)result;
    for (i = 0U; i < sizeof(*result) / sizeof(*words); ++i)
        words[i] = 0U;

    /* Exact Custom PE cleanup and Custom FPGA primitives. */
    result->cfg0_before = PE_CFG0;
    result->cfg1_before = PE_CFG1;
    result->pull0_before = PE_PULL0;
    result->data_before = PE_DATA;

    control_bus_stock_idle_cleanup();

    result->cfg0_after = PE_CFG0;
    result->cfg1_after = PE_CFG1;
    result->pull0_after = PE_PULL0;
    result->data_after = PE_DATA;

    /* Custom reproduction of the confirmed stock SMEC preamble. */
    fpga_select_command(0x67U);
    fpga_write_payload(0x08U);
    fpga_write_u16_be(0x69U, 2000U);

    (void)smec_trace_stock_transaction(
        1U, 20U, &result->job1,
        &result->job1_attempts,
        &result->job1_final_timing,
        &result->job1_outer_attempt);

    if (result->job1.completed != 0U) {
        (void)smec_trace_stock_transaction(
            2U, 64U, &result->job2,
            &result->job2_attempts,
            &result->job2_final_timing,
            &result->job2_outer_attempt);
    }

    fpga_bus_safe_idle();
}

void fpga_smec_stage29_verified_job1_test(
    struct fpga_smec_stage24_pe_result *result)
{
    uint32_t *words;
    uint32_t i;

    if (!result)
        return;

    words = (uint32_t *)(void *)result;
    for (i = 0U; i < sizeof(*result) / sizeof(*words); ++i)
        words[i] = 0U;

    /* A/B control: System_Init Part 2 intentionally skipped. */

    result->cfg0_before = PE_CFG0;
    result->cfg1_before = PE_CFG1;
    result->pull0_before = PE_PULL0;
    result->data_before = PE_DATA;

    control_bus_stock_idle_cleanup();

    result->cfg0_after = PE_CFG0;
    result->cfg1_after = PE_CFG1;
    result->pull0_after = PE_PULL0;
    result->data_after = PE_DATA;

    /* Stock SMEC_Init performs 0x67 <- 0x08 once before startup jobs. */
    fpga_select_command(0x67U);
    fpga_write_payload(0x08U);

    /* Stock SMEC_Init @ 0x4004A178..0x4004A17C preloads timing once. */
    fpga_write_u16_be(0x69U, 2000U);

    (void)smec_trace_stock_transaction(
        1U, 20U, &result->job1,
        &result->job1_attempts,
        &result->job1_final_timing,
        &result->job1_outer_attempt);

    if (result->job1.completed != 0U) {
        (void)smec_trace_stock_transaction(
            2U, 64U, &result->job2,
            &result->job2_attempts,
            &result->job2_final_timing,
            &result->job2_outer_attempt);
    }

    /* Diagnostics only after the final tested stock transaction. */
    result->idle64_after_job2 = fpga_read_selector_u8(0x64U);
    result->idle65_after_job2 = fpga_read_selector_u8(0x65U);

    /* Stage 29 intentionally stops here. No Job 3 and no capture. */
    fpga_bus_safe_idle();
}
