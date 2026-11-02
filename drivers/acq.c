#include "acq.h"
#include "fpga_bus.h"
#include "fpga_protocol.h"
#include "r11_csi.h"

#define ACQ_STREAM_QUANTITY      0x300EU
#define ACQ_CSI_IRQ_ENABLE_REG   0x01CB4070U
#define ACQ_PE_CFG0_REG          0x01C20890U
#define ACQ_PE_DATA_REG          0x01C208A0U
#define ACQ_TIMER0_CTRL_REG      0x01C20C10U

/* HW-confirmed stock 20 us/div runtime ring parameters. */
#define ACQ_RING_PERIOD          0x17FCU
#define ACQ_WINDOW_OFFSET        0x0BFDU
#define ACQ_POSITION_MARGIN      0x0003U

/*
 * Stock-confirmed HorizontalApply inputs for the fixed 20 us/div profile.
 * Stock writer 0x4003B1C4 repeatedly produced:
 *
 *   SYS_INFO+0x30/+0x34 = 0x00000000000121EA
 *
 * Preserve the stock order of integer operations because each division
 * truncates independently.
 */
#define ACQ_HF_NUMERATOR_PS      UINT64_C(10000000000)
#define ACQ_HF_TIMEBASE_PS       UINT64_C(20000000)
#define ACQ_HF_SCALE_01C         UINT64_C(95)
#define ACQ_HF_VALUE_558         UINT64_C(50)
#define ACQ_HF_VALUE_550         UINT64_C(4)
#define ACQ_HF_VALUE_554         UINT64_C(10000)
#define ACQ_HF_VALUE_55C         UINT64_C(10000)
#define ACQ_HF_DIVISOR_1B1       UINT64_C(8)
#define ACQ_HF_CONFIRMED_SOURCE  UINT64_C(0x000121EA)

/* Calculated once after READY, then re-applied before every bank read. */
static uint16_t acq_stock_capture_position = 0U;
static int acq_stock_capture_position_valid = 0;

static uint32_t read32(uintptr_t address)
{
    return *(volatile const uint32_t *)address;
}

static uint16_t acq_ring_subtract(
    uint16_t value,
    uint16_t subtract,
    uint16_t period)
{
    if (value >= subtract)
        return (uint16_t)(value - subtract);

    return (uint16_t)((uint32_t)period + value - subtract);
}

static uint64_t acq_stock_horizontal_fallback_source(void)
{
    uint64_t value;

    value = ACQ_HF_NUMERATOR_PS / ACQ_HF_TIMEBASE_PS;
    value *= ACQ_HF_SCALE_01C;
    value *= ACQ_HF_VALUE_558;
    value /= ACQ_HF_VALUE_550;
    value *= ACQ_HF_VALUE_554;
    value /= ACQ_HF_VALUE_55C;
    value /= ACQ_HF_DIVISOR_1B1;

    return value;
}

static uint16_t acq_stock_fallback_runtime_290(
    uint64_t source,
    uint16_t window_offset,
    uint16_t ring_period)
{
    uint64_t delta;

    if (ring_period == 0U)
        return 0U;

    /* Stock fallback clamps to zero if the 64-bit source is too small. */
    if (source < (uint64_t)window_offset)
        return 0U;

    delta = source - (uint64_t)window_offset;
    return (uint16_t)(delta % (uint64_t)ring_period);
}

static int acq_compute_stock_capture_position(
    struct acq_prepare_result *result)
{
    uint16_t live_pointer;
    uint16_t runtime_290;
    uint16_t runtime_63c;
    uint16_t position;

    result->pointer_valid =
        fpga_protocol_capture_pointer_valid() ? 1 : 0;

    if (result->pointer_valid) {
        /* Stock live branch: selector 0x14 supplies the BE16 pointer. */
        live_pointer = fpga_protocol_capture_read_pointer();
        runtime_290 = acq_ring_subtract(
            live_pointer,
            ACQ_WINDOW_OFFSET,
            ACQ_RING_PERIOD);
    } else {
        uint64_t fallback_source;

        /*
         * Stock fallback branch: selector 0x14 is not read. HorizontalApply
         * provides a 64-bit CPU-side source, which is normalized into the
         * same acquisition ring before the common follow-up calculation.
         */
        fallback_source = acq_stock_horizontal_fallback_source();
        if (fallback_source != ACQ_HF_CONFIRMED_SOURCE) {
            acq_stock_capture_position_valid = 0;
            return 0;
        }

        live_pointer = 0U;
        runtime_290 = acq_stock_fallback_runtime_290(
            fallback_source,
            ACQ_WINDOW_OFFSET,
            ACQ_RING_PERIOD);
    }

    runtime_63c = acq_ring_subtract(
        runtime_290,
        ACQ_WINDOW_OFFSET,
        ACQ_RING_PERIOD);
    position = acq_ring_subtract(
        runtime_63c,
        ACQ_POSITION_MARGIN,
        ACQ_RING_PERIOD);

    result->live_pointer = live_pointer;
    result->runtime_290 = runtime_290;
    result->runtime_63c = runtime_63c;
    result->capture_position = position;

    acq_stock_capture_position = position;
    acq_stock_capture_position_valid = 1;
    __asm__ volatile ("dmb sy" ::: "memory");
    return 1;
}

void acq_configure_stream(void)
{
    /* Current HW-confirmed receiver profile: 0x1800-byte payload. */
    r11_csi_configure_fixed_stream_geometry();
    fpga_protocol_stream_set_quantity(ACQ_STREAM_QUANTITY);
    __asm__ volatile ("dmb sy" ::: "memory");
}

int acq_prepare(
    uint8_t mode,
    uint32_t max_polls,
    struct acq_prepare_result *result)
{
    struct acq_prepare_result local;
    struct acq_prepare_result *out = result ? result : &local;
    struct fpga_protocol_ready_result ready;

    out->status_before = 0U;
    out->status_after = 0U;
    out->polls = 0U;
    out->live_pointer = 0U;
    out->runtime_290 = 0U;
    out->runtime_63c = 0U;
    out->capture_position = 0U;
    out->pointer_valid = 0;
    out->ready = 0;
    acq_stock_capture_position = 0U;
    acq_stock_capture_position_valid = 0;

    /* Strict stock order: arm first, then poll selector 0x0A. */
    fpga_protocol_acq_prepare(mode);
    fpga_protocol_acq_wait_ready(max_polls, &ready);

    out->status_before = ready.first_status;
    out->status_after = ready.final_status;
    out->polls = ready.polls;
    out->ready = ready.ready;

    if (!out->ready)
        return 0;

    return acq_compute_stock_capture_position(out);
}

int acq_capture(
    uintptr_t buffer,
    uint16_t position,
    uint8_t bank,
    uint32_t timeout_iterations,
    struct acq_capture_result *result)
{
    struct acq_capture_result local;
    struct acq_capture_result *out = result ? result : &local;
    uint32_t status_after = 0U;

    out->status_before = 0U;
    out->status_after = 0U;
    out->mode = 0U;
    out->buffer0 = 0U;
    out->irq_enable = 0U;
    out->wait_polls = 0U;
    out->pe_data_before = 0U;
    out->pe_data_high = 0U;
    out->pe_data_low = 0U;
    out->timer_before = 0U;
    out->timer_low = 0U;
    out->timer_high = 0U;
    out->pe_cfg0_csi = 0U;
    out->pe_cfg0_gpio = 0U;
    out->done = 0;

    /*
     * The old caller-supplied fixed position (historically 3) is deliberately
     * ignored. Stock derives one position after READY and re-applies it before
     * every bank transfer.
     */
    (void)position;
    if (!acq_stock_capture_position_valid)
        return 0;

    fpga_protocol_capture_prepare_position(acq_stock_capture_position);
    fpga_protocol_capture_select_bank(bank);
    out->pe_data_before = read32(ACQ_PE_DATA_REG);
    fpga_bus_enter_csi_mode();
    out->pe_cfg0_csi = read32(ACQ_PE_CFG0_REG);

    r11_csi_set_buffer0(buffer);
    r11_csi_ack(R11_CSI_DONE);

    out->status_before = r11_csi_status();

    r11_csi_timer0_gate(0);
    r11_csi_capture_enable();
    fpga_capture_gate(1);

    out->done = r11_csi_wait_done_bounded(
        timeout_iterations, &status_after) ? 1 : 0;

    fpga_capture_gate(0);
    out->pe_data_low = read32(ACQ_PE_DATA_REG);
    r11_csi_timer0_gate(1);
    out->timer_high = read32(ACQ_TIMER0_CTRL_REG);
    r11_csi_ack(R11_CSI_DONE);
    /* Stock CSI mode auto-clears after a completed frame. */
    fpga_bus_leave_csi_mode();
    out->pe_cfg0_gpio = read32(ACQ_PE_CFG0_REG);

    out->status_after = status_after;
    out->mode = r11_csi_mode();
    out->buffer0 = r11_csi_buffer0();
    out->irq_enable = read32(ACQ_CSI_IRQ_ENABLE_REG);
    out->wait_polls = out->done ? 1U : timeout_iterations;

    return out->done;
}
