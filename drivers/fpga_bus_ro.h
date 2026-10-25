#ifndef FPGA_BUS_RO_H
#define FPGA_BUS_RO_H

#include <stdint.h>

struct fpga_diag_result {
    uint8_t raw[5];
    uint32_t id_word;
    uint32_t id_word_shifted;
    uint32_t pe_cfg0_before;
    uint32_t pe_cfg1_before;
    uint32_t pe_data_before;
    uint32_t pe_cfg0_after;
    uint32_t pe_cfg1_after;
    uint32_t pe_data_after;
    int present;
};

void fpga_bus_safe_idle(void);
void fpga_diag_read_id(struct fpga_diag_result *result);
void fpga_diag_read_id_baseline(struct fpga_diag_result *result);

struct fpga_capture_arm_probe_result {
    uint32_t pe_cfg0_before;
    uint32_t pe_cfg1_before;
    uint32_t pe_data_before;
    uint32_t pe_cfg0_after;
    uint32_t pe_cfg1_after;
    uint32_t pe_data_after;
};

/*
 * Reproduce only the confirmed EXE2 command-0x08 transition:
 *   select 0x08, write 0, select 0x08, write 1.
 * No bank select, no CSI enable and no polling are performed here.
 */
void fpga_capture_arm_probe(struct fpga_capture_arm_probe_result *result);

/* Exact stock bus primitives: command 0x15, payload 0x00. */
void fpga_select_bank0(void);
/* Stock per-bank CSI setup: cyclic position 0 through 0x20/0x21, then 0x1F. */
void fpga_prepare_csi_bank0(void);
void fpga_prepare_csi_bank0_position(uint16_t position);
void fpga_set_stream_quantity(uint16_t quantity);
/* Shared PE interface lifecycle around one CSI capture. */
void fpga_bus_enter_csi_mode(void);
void fpga_bus_leave_csi_mode(void);
void fpga_capture_gate(int high);

struct fpga_stream_activity_result {
    uint32_t first;
    uint32_t last;
    uint32_t changed_mask;
    uint32_t high_mask;
    uint32_t low_mask;
    uint32_t samples;
};
/* Samples PE0, PE2, PE3 and PE8..PE15 as GPIO inputs while PE1 is high. */
void fpga_stream_activity_probe(uint32_t samples,
                                struct fpga_stream_activity_result *result);

struct fpga_acq_ready_result {
    uint32_t polls;
    uint8_t final_status;
    uint8_t mode_byte;
    int ready;
};
/* Safe upstream probe only: no CSI pinmux, no PE1 gate, no RAM transfer. */
void fpga_acq_ready_probe(uint8_t mode_byte, uint32_t max_polls,
                          struct fpga_acq_ready_result *result);

struct fpga_acq_position_result {
    struct fpga_acq_ready_result ready;
    uint8_t valid_status;
    uint16_t position;
    int valid;
};
/* Upstream preparation plus stock 0x18/0x14 position read, still no CSI. */
void fpga_acq_position_probe(uint8_t mode_byte, uint32_t max_polls,
                             struct fpga_acq_position_result *result);

#endif
