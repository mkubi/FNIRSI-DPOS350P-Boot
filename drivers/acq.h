#ifndef ACQ_H
#define ACQ_H

#include <stdint.h>
#include <stddef.h>

/* Configure fixed CSI geometry and FPGA stream quantity once per acquisition session. */
void acq_configure_stream(void);

struct acq_prepare_result {
    uint8_t status_before;
    uint8_t status_after;
    uint32_t polls;
    int ready;
};

int acq_prepare(
    uint8_t mode,
    uint32_t max_polls,
    struct acq_prepare_result *result);

struct acq_capture_result {
    uint32_t status_before;
    uint32_t status_after;
    uint32_t mode;
    uint32_t buffer0;
    uint32_t irq_enable;
    uint32_t wait_polls;
    uint32_t pe_data_before;
    uint32_t pe_data_high;
    uint32_t pe_data_low;
    uint32_t timer_before;
    uint32_t timer_low;
    uint32_t timer_high;
    uint32_t pe_cfg0_csi;
    uint32_t pe_cfg0_gpio;
    int done;
};

/*
 * Run one acquisition transfer into buffer.
 *
 * The public interface is bank/position agnostic. The current receiver
 * geometry and stream quantity are the HW-confirmed DPOS350P profile.
 */
int acq_capture(
    uintptr_t buffer,
    uint16_t position,
    uint8_t bank,
    uint32_t timeout_iterations,
    struct acq_capture_result *result);

#endif
