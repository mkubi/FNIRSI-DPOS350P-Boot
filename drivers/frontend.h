#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdint.h>

enum frontend_step {
    FRONTEND_STEP_NONE = 0,
    FRONTEND_STEP_SHARED = 1,
    FRONTEND_STEP_CH1_RANGE,
    FRONTEND_STEP_CH1_JOB_08,
    FRONTEND_STEP_CH1_JOB_0A,
    FRONTEND_STEP_CH1_COMMIT,
    FRONTEND_STEP_CH1_DIRECT,
    FRONTEND_STEP_CH2_RANGE,
    FRONTEND_STEP_CH2_JOB_09,
    FRONTEND_STEP_CH2_JOB_0B,
    FRONTEND_STEP_CH2_COMMIT,
    FRONTEND_STEP_CH2_DIRECT,
    FRONTEND_STEP_CH1_COUPLING,
    FRONTEND_STEP_CH1_BANDWIDTH,
    FRONTEND_STEP_CH2_COUPLING,
    FRONTEND_STEP_CH2_BANDWIDTH,
    FRONTEND_STEP_CHANNEL_ROUTING
};

struct frontend_init_result {
    uint32_t completed_steps;
    uint32_t failed_step;
    uint32_t last_job_result;
    uint32_t last_job_attempts;
    int last_job_error;
    int success;
};

/*
 * Apply the HW-confirmed stock default frontend state once:
 *
 *   CH1: 200 mV/div, enabled
 *   CH2: 200 mV/div, configured but disabled
 *   CH1/CH2 bandwidth: stored as 350 MHz
 *   CH1/CH2 coupling selector: 0x00
 *
 * No periodic frontend refresh is performed.
 */
int frontend_init_stock_default(struct frontend_init_result *result);

#endif
