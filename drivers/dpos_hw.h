#ifndef DPOS_HW_H
#define DPOS_HW_H

#include <stdint.h>

struct dpos_hw_init_result {
    uint32_t fpga_id_before;
    uint32_t fpga_id_after;
    uint32_t clock_writes;
    uint32_t csi_if_cfg;
    uint32_t csi_hsize;
    uint32_t csi_buffer0;
    uint32_t csi_irq_enable;
    int fpga_before;
    int csi_ready;
    int clocks_ready;
    int fpga_after;
};

/*
 * Initialize the HW-confirmed FPGA/CSI prerequisite layers.
 *
 * This is a custom implementation and does not call stock EXE2 code.
 * Acquisition preparation, SMEC and capture remain separate operations.
 */
int dpos_hw_init(struct dpos_hw_init_result *result);

#endif
