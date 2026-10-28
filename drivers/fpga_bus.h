#ifndef FPGA_BUS_H
#define FPGA_BUS_H

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

/*
 * Production low-level FPGA bus API.
 *
 * These functions expose only the HW-confirmed physical PE bus operations.
 * Selector semantics and multi-byte framing belong to the protocol layer.
 */
void fpga_bus_select(uint8_t selector);
void fpga_bus_write(uint8_t value);
uint8_t fpga_bus_read(void);

/* Temporary compatibility wrappers for existing DPOSLAB diagnostics. */
void fpga_test_custom_select_command(uint8_t selector);
void fpga_test_custom_write_payload(uint8_t value);
uint8_t fpga_test_custom_read_byte(void);
void fpga_diag_read_id(struct fpga_diag_result *result);
void fpga_diag_read_id_baseline(struct fpga_diag_result *result);
/* Exact stock 0x400492E4 ID transaction: selector 0x01 and exactly four
 * reads, with no diagnostic fifth read and no added settle delay. */
void fpga_diag_read_id_stock_raw4(struct fpga_diag_result *result);

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
/* Exact stock per-bank selector: FPGA register 0x1F <- bank 0. */
void fpga_select_csi_bank0(void);
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
/* Read only stock 0x18 validity and 0x14 position.
 * Does not run acquisition prepare and does not poll 0x0A. */
void fpga_acq_position_read_after_ready(
    struct fpga_acq_position_result *result);

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

/* HW-confirmed SMEC_Init startup baseline, jobs 1..7, with bounded polling. */
int fpga_smec_init_baseline(struct fpga_smec_init_result *result);

#define FPGA_SMEC_TRACE_VALUES 16U

struct fpga_smec_poll_trace {
    uint32_t count;
    uint32_t changes;
    uint32_t bit0_count;
    uint8_t first;
    uint8_t last;
    uint8_t or_value;
    uint8_t value_count;
    uint8_t values[FPGA_SMEC_TRACE_VALUES];
};

struct fpga_smec_job_trace {
    uint8_t index;
    uint8_t echo68;
    uint8_t status67;
    uint8_t completed;
    uint16_t timing;
    uint16_t reserved;
    uint32_t value;
    uint32_t result66;
    int error;
    struct fpga_smec_poll_trace poll64;
    struct fpga_smec_poll_trace poll65;
};

struct fpga_smec_job2_investigation {
    struct fpga_smec_job_trace job1;
    struct fpga_smec_job_trace job2;
};

/* Isolated one-shot jobs 1 and 2, no retries and no CSI side effects. */
void fpga_smec_investigate_job2(struct fpga_smec_job2_investigation *result);

struct fpga_smec_job2_recovery {
    struct fpga_smec_job_trace job1;
    struct fpga_smec_job_trace job2_normal;
    struct fpga_smec_job_trace job2_rearmed;
    uint8_t idle64_after_job1;
    uint8_t idle65_after_job1;
    uint8_t idle64_after_fail;
    uint8_t idle65_after_fail;
};

/* Job 2 normal attempt followed, only on failure, by 0x67<-8 re-arm retry. */
void fpga_smec_debug_job2_recovery(struct fpga_smec_job2_recovery *result);

struct fpga_smec_stage24_pe_result {
    struct fpga_smec_job_trace job1;
    struct fpga_smec_job_trace job2;
    uint32_t cfg0_before;
    uint32_t cfg1_before;
    uint32_t pull0_before;
    uint32_t data_before;
    uint32_t cfg0_after;
    uint32_t cfg1_after;
    uint32_t pull0_after;
    uint32_t data_after;
    uint8_t idle64_after_job1;
    uint8_t idle65_after_job1;
    uint8_t idle64_after_job2;
    uint8_t idle65_after_job2;
    uint8_t job2_echo_first;
    uint8_t job2_echo_last;
    uint8_t job2_echo_or;
    uint8_t job2_echo_changes;
    uint32_t job2_echo_count;
    uint32_t job2_echo_matched;
    uint32_t job1_attempts;
    uint32_t job1_final_timing;
    uint32_t job1_outer_attempt;
    uint32_t job2_attempts;
    uint32_t job2_final_timing;
    uint32_t job2_outer_attempt;
    uint32_t job2_p64_saw_zero;
    uint32_t job2_p64_zero_polls;
    uint32_t job2_p64_ready_polls;
    uint32_t job2_p65_saw_zero;
    uint32_t job2_p65_zero_polls;
    uint32_t job2_p65_ready_polls;
    uint32_t part2_ccu_before;
    uint32_t part2_ccu_after;
    uint32_t part2_div_before;
    uint32_t part2_div_after;
    uint32_t part2_ok;

};

/* Stage 24: reproduce the exact stock PE control-bus idle baseline, then
 * submit one Job 1 and one Job 2 attempt. No retry, re-arm, CSI or DMA. */
void fpga_smec_stage24_pe_cleanup_test(
    struct fpga_smec_stage24_pe_result *result);

void fpga_smec_stage29_verified_job1_test(
    struct fpga_smec_stage24_pe_result *result);

/* Test A: Custom SMEC transport after a caller-provided stock FPGA_Start. */
void fpga_smec_custom_job12_test(
    struct fpga_smec_stage24_pe_result *result);

#endif
