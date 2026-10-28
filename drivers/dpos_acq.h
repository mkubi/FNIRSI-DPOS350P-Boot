#ifndef DPOS_ACQ_H
#define DPOS_ACQ_H

#include <stdint.h>
#include <stddef.h>

struct dpos_acq_capture_result {
    uint32_t status_before;
    uint32_t status_after;
    uint32_t mode;
    uint32_t buffer0;
    uint32_t irq_enable;
    uint32_t wait_polls;
    int done;
};

/*
 * Run one acquisition transfer into buffer.
 *
 * The public interface is bank/position agnostic. The current receiver
 * geometry and stream quantity are the HW-confirmed DPOS350P profile.
 */
int dpos_acq_capture(
    uintptr_t buffer,
    uint16_t position,
    uint8_t bank,
    uint32_t timeout_iterations,
    struct dpos_acq_capture_result *result);

#endif
