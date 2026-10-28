#ifndef FPGA_SMEC_H
#define FPGA_SMEC_H

#include <stdint.h>

struct fpga_smec_init_result {
    uint32_t completed_jobs;
    uint32_t attempts;
    uint32_t poll64_count;
    uint32_t poll65_count;
    uint32_t result66;
    uint16_t timing;
    uint8_t current_job;
    uint8_t status64;
    uint8_t final65;
    uint8_t echo68;
    uint8_t status67;
    int success;
    int error;
};

/* HW-confirmed fixed-profile SMEC initialization, jobs 1 through 7. */
int fpga_smec_init(struct fpga_smec_init_result *result);

#endif
