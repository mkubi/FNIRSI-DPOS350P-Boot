#ifndef SCOPE_BACKEND_H
#define SCOPE_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

enum scope_channel {
    SCOPE_CHANNEL_1 = 0,
    SCOPE_CHANNEL_2 = 1,
    SCOPE_CHANNEL_COUNT = 2
};

enum scope_channel_mask {
    SCOPE_CHANNEL_MASK_NONE = 0U,
    SCOPE_CHANNEL_MASK_CH1 = 1U << SCOPE_CHANNEL_1,
    SCOPE_CHANNEL_MASK_CH2 = 1U << SCOPE_CHANNEL_2,
    SCOPE_CHANNEL_MASK_BOTH = SCOPE_CHANNEL_MASK_CH1 | SCOPE_CHANNEL_MASK_CH2
};

enum scope_coupling {
    SCOPE_COUPLING_DC,
    SCOPE_COUPLING_AC
};

enum scope_trigger_mode {
    SCOPE_TRIGGER_AUTO,
    SCOPE_TRIGGER_NORMAL,
    SCOPE_TRIGGER_SINGLE
};

enum scope_trigger_edge {
    SCOPE_TRIGGER_RISING,
    SCOPE_TRIGGER_FALLING
};

enum scope_timebase_id {
    SCOPE_TIMEBASE_20_US,
    SCOPE_TIMEBASE_COUNT
};

enum scope_capture_status {
    SCOPE_CAPTURE_OK = 0,
    SCOPE_CAPTURE_WAITING,
    SCOPE_CAPTURE_TIMEOUT,
    SCOPE_CAPTURE_NOT_TRIGGERED,
    SCOPE_CAPTURE_INVALID_CONFIG,
    SCOPE_CAPTURE_NOT_CALIBRATED,
    SCOPE_CAPTURE_FPGA_ERROR,
    SCOPE_CAPTURE_CSI_ERROR,
    SCOPE_CAPTURE_CONVERSION_ERROR,
    SCOPE_CAPTURE_INTERNAL_ERROR
};

enum scope_backend_state {
    SCOPE_BACKEND_UNINITIALIZED,
    SCOPE_BACKEND_IDLE,
    SCOPE_BACKEND_CONFIGURED,
    SCOPE_BACKEND_ARMED,
    SCOPE_BACKEND_WAITING_READY,
    SCOPE_BACKEND_CAPTURING,
    SCOPE_BACKEND_PROCESSING,
    SCOPE_BACKEND_RESULT_READY,
    SCOPE_BACKEND_ERROR
};

enum scope_backend_error_stage {
    SCOPE_ERROR_STAGE_NONE,
    SCOPE_ERROR_STAGE_VALIDATE,
    SCOPE_ERROR_STAGE_FPGA_INIT,
    SCOPE_ERROR_STAGE_SMEC,
    SCOPE_ERROR_STAGE_GEOMETRY,
    SCOPE_ERROR_STAGE_PROFILE,
    SCOPE_ERROR_STAGE_FRONTEND,
    SCOPE_ERROR_STAGE_TRIGGER,
    SCOPE_ERROR_STAGE_REARM,
    SCOPE_ERROR_STAGE_READY,
    SCOPE_ERROR_STAGE_CSI_SETUP,
    SCOPE_ERROR_STAGE_CAPTURE,
    SCOPE_ERROR_STAGE_CACHE,
    SCOPE_ERROR_STAGE_CALIBRATION,
    SCOPE_ERROR_STAGE_CONVERSION
};

enum scope_capture_flags {
    SCOPE_CAPTURE_FLAG_NONE = 0U,
    SCOPE_CAPTURE_FLAG_RAW_DIAGNOSTIC = 1U << 0,
    SCOPE_CAPTURE_FLAG_NO_CALIBRATION = 1U << 1,
    SCOPE_CAPTURE_FLAG_SINGLE_SLOT = 1U << 2
};

struct scope_channel_config {
    bool enabled;
    enum scope_coupling coupling;
    bool bandwidth_limit_enabled;
    uint32_t volts_per_div_uv;
    int32_t vertical_position_uv;
    uint32_t probe_ratio_numerator;
    uint32_t probe_ratio_denominator;
};

struct scope_timebase_config {
    enum scope_timebase_id id;
    uint32_t requested_sample_count;
    uint16_t horizontal_position_per_mille;
};

struct scope_trigger_config {
    enum scope_trigger_mode mode;
    enum scope_channel source;
    enum scope_trigger_edge edge;
    int32_t level_uv;
    uint16_t horizontal_position_per_mille;
    uint32_t auto_timeout_ms;
};

struct scope_capture_request {
    struct scope_channel_config channel[SCOPE_CHANNEL_COUNT];
    struct scope_timebase_config timebase;
    struct scope_trigger_config trigger;
    uint32_t flags;
};

struct scope_waveform {
    uint16_t *samples;
    uint32_t capacity;
    uint32_t sample_count;
    uint64_t sample_period_fs;
    uint16_t zero_code;
    int32_t microvolts_per_code;
    uint16_t minimum;
    uint16_t maximum;
    uint32_t average;
    bool valid;
};

struct scope_capture_result {
    enum scope_capture_status status;
    struct scope_waveform channel[SCOPE_CHANNEL_COUNT];
    uint32_t active_channel_mask;
    uint32_t trigger_index;
    uint32_t capture_sequence;
    uint32_t diagnostic_flags;
};

struct scope_backend_info {
    uint32_t channel_count;
    uint32_t maximum_samples;
    bool fpga_present;
    bool csi_present;
    bool calibration_loaded;
    uint32_t supported_channel_masks;
    uint32_t supported_timebase_mask;
};

struct scope_backend_diagnostics {
    enum scope_backend_state state;
    enum scope_capture_status last_status;
    enum scope_backend_error_stage first_error_stage;
    uint8_t fpga_acquisition_status_before;
    uint8_t fpga_acquisition_status_after;
    uint8_t fpga_runtime_status;
    uint8_t raw_minimum;
    uint8_t raw_maximum;
    uint16_t prepared_capture_position;
    uint16_t live_capture_pointer;
    uint16_t fpga_stream_quantity;
    uint16_t runtime_position_base;
    uint32_t ready_poll_count;
    uint32_t capture_poll_count;
    uint32_t csi_status_before;
    uint32_t csi_status_after;
    uint32_t csi_buffer0;
    uint32_t csi_geometry;
    uint32_t raw_changed_bytes;
    uint32_t raw_nonzero_bytes;
    uint32_t raw_unique_values;
    uint32_t raw_hash;
    uint32_t raw_first_changed;
    uint32_t raw_last_changed;
    uint32_t capture_sequence;
};

int scope_backend_init(struct scope_backend_info *info);
void scope_backend_deinit(void);
int scope_backend_configure(const struct scope_capture_request *request);
int scope_backend_start(void);
enum scope_capture_status scope_backend_poll(struct scope_capture_result *result);
int scope_backend_stop(void);
enum scope_capture_status scope_backend_capture_once(
    const struct scope_capture_request *request,
    struct scope_capture_result *result,
    uint32_t timeout_ms);
void scope_backend_get_diagnostics(struct scope_backend_diagnostics *diagnostics);
const uint8_t *scope_backend_raw_data(uint32_t *size);

#endif
