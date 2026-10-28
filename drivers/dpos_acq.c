#include "dpos_acq.h"
#include "fpga_bus.h"
#include "fpga_protocol.h"
#include "r11_csi.h"

#define DPOS_ACQ_STREAM_QUANTITY 0x300EU
#define DPOS_ACQ_CSI_IRQ_ENABLE_REG 0x01CB4070U

static uint32_t read32(uintptr_t address)
{
    return *(volatile const uint32_t *)address;
}

static void dpos_acq_configure_stream(void)
{
    /* Current HW-confirmed receiver profile: 0x1800-byte payload. */
    r11_csi_configure_fixed_stream_geometry();
    fpga_protocol_stream_set_quantity(DPOS_ACQ_STREAM_QUANTITY);
    __asm__ volatile ("dmb sy" ::: "memory");
}

int dpos_acq_prepare(
    uint8_t mode,
    uint32_t max_polls,
    struct dpos_acq_prepare_result *result)
{
    struct dpos_acq_prepare_result local;
    struct dpos_acq_prepare_result *out = result ? result : &local;
    struct fpga_protocol_ready_result ready;

    out->status_before = fpga_protocol_read_u8(0x0AU);
    out->status_after = 0U;
    out->polls = 0U;
    out->ready = 0;

    fpga_protocol_acq_prepare(mode);
    fpga_protocol_acq_wait_ready(max_polls, &ready);

    out->status_after = ready.final_status;
    out->polls = ready.polls;
    out->ready = ready.ready;

    return out->ready;
}

int dpos_acq_capture(
    uintptr_t buffer,
    uint16_t position,
    uint8_t bank,
    uint32_t timeout_iterations,
    struct dpos_acq_capture_result *result)
{
    struct dpos_acq_capture_result local;
    struct dpos_acq_capture_result *out = result ? result : &local;
    uint32_t status_after = 0U;

    out->status_before = 0U;
    out->status_after = 0U;
    out->mode = 0U;
    out->buffer0 = 0U;
    out->irq_enable = 0U;
    out->wait_polls = 0U;
    out->done = 0;

    dpos_acq_configure_stream();
    fpga_protocol_capture_prepare_position(position);
    fpga_protocol_capture_select_bank(bank);
    fpga_bus_enter_csi_mode();

    r11_csi_set_buffer0(buffer);
    r11_csi_ack(R11_CSI_DONE);

    out->status_before = r11_csi_status();

    r11_csi_timer0_gate(0);
    r11_csi_capture_enable();
    fpga_capture_gate(1);

    out->done = r11_csi_wait_done_bounded(
        timeout_iterations, &status_after) ? 1 : 0;

    fpga_capture_gate(0);
    r11_csi_timer0_gate(1);
    r11_csi_ack(R11_CSI_DONE);
    r11_csi_capture_disable();
    fpga_bus_leave_csi_mode();

    out->status_after = status_after;
    out->mode = r11_csi_mode();
    out->buffer0 = r11_csi_buffer0();
    out->irq_enable = read32(DPOS_ACQ_CSI_IRQ_ENABLE_REG);
    out->wait_polls = out->done ? 1U : timeout_iterations;

    return out->done;
}
