#ifndef DPOSLAB_ACQ_PREREQ_PROBE_H
#define DPOSLAB_ACQ_PREREQ_PROBE_H

#include <stdint.h>

struct dposlab_acq_probe_result {
    uint32_t raw0a;
    uint32_t prepared0a;
    uint32_t polls;
    uint32_t ready;
    uint32_t capture_called;
    uint32_t capture_succeeded;
    uint32_t csi_wait_status;
    uint32_t csi_wait_mode;
    uint32_t csi_wait_buffer0;
    uint32_t csi_wait_hw_done;
    uint32_t stream_a_640;
    uint32_t stream_b_6f2;
    uint32_t stream_geometry;
    uint32_t stream_quantity;
    uint32_t stream_payload_guess;
    uint32_t bank0_hash_before;
    uint32_t bank0_hash_after;
    uint32_t bank0_changed_bytes;
    uint32_t bank0_first_change;
    uint32_t bank0_last_change;
    uint32_t bank0_data0;
    uint32_t bank0_data1;
    uint32_t bank0_data2;
    uint32_t bank0_data3;
};

extern struct dposlab_acq_probe_result dposlab_acq_probe_result;
void dposlab_acq_variant1_probe(void);
void dposlab_acq_variant1_monitor(void);

#endif
