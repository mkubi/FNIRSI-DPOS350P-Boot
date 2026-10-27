#ifndef DPOSLAB_CSI_BANK0_PROBE_H
#define DPOSLAB_CSI_BANK0_PROBE_H

#include <stdint.h>

struct dposlab_csi_bank0_result {
    uint32_t ready_before;
    uint32_t position_valid;
    uint32_t position;
    uint32_t position_polls;
    uint32_t buffer_reg;
    uint32_t status_before;
    uint32_t status_after;
    uint32_t mode_active;
    uint32_t mode_after;
    uint32_t hash_before;
    uint32_t hash_after;
    uint32_t changed_bytes;
    uint32_t changed_blocks;
    uint32_t first_change;
    uint32_t last_change;
    uint32_t data0;
    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
    uint32_t left_guard_ok;
    uint32_t right_guard_ok;
    uint32_t done;
    uint32_t attempted;
};

extern struct dposlab_csi_bank0_result dposlab_csi_bank0_result;
void dposlab_csi_bank0_capture(uint8_t mode_byte, int ready_before);

#endif
