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

#endif
