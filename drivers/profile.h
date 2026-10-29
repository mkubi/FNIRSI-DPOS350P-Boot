#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

#define PROFILE_STEP_TIMEBASE (1U << 0)
#define PROFILE_STEP_MODE      (1U << 1)
#define PROFILE_STEP_TIMING      (1U << 2)
#define PROFILE_STEP_LAYOUT    (1U << 3)
#define PROFILE_STEP_FPGA_APPLY    (1U << 4)
#define PROFILE_STEP_CHANNEL_ORDER    (1U << 5)
#define PROFILE_STEP_CHANNEL_CAL    (1U << 6)
#define PROFILE_STEPS_ALL       0x0000007FU

/* Detailed progress IDs inside fpga_profile_apply(). */
#define PROFILE_PROGRESS_PARAM_21   0x00000121U
#define PROFILE_PROGRESS_COMMIT_C1  0x000001C1U
#define PROFILE_PROGRESS_SELECT_04  0x00000204U
#define PROFILE_PROGRESS_PARAM_22   0x00000122U
#define PROFILE_PROGRESS_COMMIT_C2  0x000001C2U
#define PROFILE_PROGRESS_SELECT_05  0x00000205U
#define PROFILE_PROGRESS_PARAM_23   0x00000123U
#define PROFILE_PROGRESS_COMMIT_C3  0x000001C3U
#define PROFILE_PROGRESS_SELECT_06  0x00000206U
#define PROFILE_PROGRESS_SELECT_02  0x00000202U
#define PROFILE_PROGRESS_SELECT_0D  0x0000020DU
#define PROFILE_PROGRESS_PULSE_07   0x00000207U
#define PROFILE_PROGRESS_AVS_DELAY  0x00000301U

struct profile_init_result {
    uint32_t completed_steps;
    uint32_t failed_step;
    int success;
    int error;
};

typedef void (*profile_progress_fn)(
    uint32_t completed_steps,
    uint32_t current_step);

/* Initialize the fixed HW-confirmed standalone acquisition profile. */
int profile_init(
    struct profile_init_result *result,
    profile_progress_fn progress);

#endif
