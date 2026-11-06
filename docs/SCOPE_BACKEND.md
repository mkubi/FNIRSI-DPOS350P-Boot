# DPOS350P Scope Backend

**Status:** Design draft  
**Updated:** 2026-11-03  
**Target repository:** `FNIRSI-DPOS350P-Boot`  
**Proposed path:** `docs/SCOPE_BACKEND.md`

## 1. Purpose

This document defines the intended software boundary between a scope application and the DPOS350P-specific hardware implementation.

The immediate goal is a narrow but complete vertical path:

```text
scope capture request
    -> DPOS350P frontend/profile configuration
    -> FPGA acquisition and runtime rearm
    -> R11 CSI raw capture
    -> calibration selection
    -> raw-lane assembly and conversion
    -> calibrated waveform result
```

The first implementation only needs to support the current hardware-confirmed CH1 fixed profile. The API must nevertheless avoid exposing experiment-local names, stock RAM offsets, FPGA selectors, CSI registers, raw-slot layout, or calibration-matrix details to the scope application.

This document is an implementation design. Reverse-engineering facts remain authoritative in the companion `FNIRSI-DPOS350P-Hack` repository:

```text
docs/SYS_INFO.md
    canonical stock state names, offsets, widths, and evidence levels

docs/ACQUISITION.md
    canonical acquisition, runtime, raw-ring, and calibration behavior

docs/FPGA_PROTOCOL.md
    canonical ARM-visible FPGA protocol semantics

docs/FPGA.md
    chronological FPGA/CSI reverse-engineering record
```

## 2. Design principles

### 2.1 Scope-facing API uses oscilloscope concepts

The scope application may use:

```text
channel enable
coupling
bandwidth limit
V/div
vertical position
probe ratio
timebase
trigger source, edge, mode, and level
requested sample count
RUN, STOP, and SINGLE
waveform samples and physical metadata
```

The scope application must not use:

```text
stock SYS_INFO offsets
FPGA command or selector numbers
SMEC job identifiers
R11 CSI register addresses
raw bank stride or destination encoding
raw lane slot arrays
ring-origin arithmetic
calibration matrix indices
```

### 2.2 Stock names remain traceable

Private DPOS350P types should use the canonical implementation-oriented names from HACK `SYS_INFO.md`.

Example:

```c
uint16_t runtime_position_base;
/* Stock equivalent: SYS_RUNTIME_POSITION_BASE (+0x6F2). */
```

Do not use new names such as `field_6f2`, `value_28c`, or `state_6ec` in production code.

### 2.3 Confirmed role is not the same as confirmed physical meaning

Names such as:

```text
raw_lane_mode
acquisition_window_value_a
calibration_selector_base
```

are acceptable because they describe a confirmed software role. They must not be renamed to stronger physical concepts such as ADC phase, gain, offset, nanoseconds, or volts until hardware tests prove those meanings.

### 2.4 Unsupported configurations fail explicitly

The first backend version must reject unsupported combinations instead of silently selecting an approximate profile.

Examples:

```text
unsupported timebase
unsupported V/div
unsupported dual-channel profile
unsupported trigger mode
missing calibration
```

### 2.5 All waits are bounded

Every FPGA READY poll, CSI completion wait, SMEC transaction, and acquisition wait must have a finite timeout and a diagnostic exit reason.

### 2.6 Diagnostics are a stable feature

Diagnostics must not be temporary `printf` output. The same backend should support:

```text
normal scope application
standalone Custom App tests
DPOSLAB A/B experiments
regression testing of new profiles
```

## 3. Architectural boundary

```text
+------------------------------------------------------+
| Scope application                                    |
|                                                      |
| state machine, UI, rendering, measurements, cursors  |
+---------------------------+--------------------------+
                            |
                            | scope_backend.h
                            v
+------------------------------------------------------+
| Generic scope backend contract                       |
|                                                      |
| configure, start, poll, stop, capture_once            |
| calibrated CH1/CH2 waveform result                   |
+---------------------------+--------------------------+
                            |
                            | private DPOS350P adapter
                            v
+------------------------------------------------------+
| DPOS350P implementation                              |
|                                                      |
| frontend | profile | acquisition | CSI | calibration |
| raw-lane assembly | calibrated conversion            |
+-------------+----------------------------+-----------+
              |                            |
              v                            v
       GOWIN FPGA / SMEC              R11 CSI / RAM
```

The intended future use is either:

1. a new DPOS350P-specific scope frontend; or
2. an adapted 1013D scope application using this generic contract.

The 1013D-derived application must not directly call DPOS350P FPGA or CSI functions.

## 4. Public API overview

Proposed public header:

```text
scope/scope_backend.h
```

Initial public API:

```c
int scope_backend_init(struct scope_backend_info *info);
void scope_backend_deinit(void);

int scope_backend_configure(
    const struct scope_capture_request *request);

int scope_backend_start(void);

enum scope_capture_status scope_backend_poll(
    struct scope_capture_result *result);

int scope_backend_stop(void);

enum scope_capture_status scope_backend_capture_once(
    const struct scope_capture_request *request,
    struct scope_capture_result *result,
    uint32_t timeout_ms);

void scope_backend_get_diagnostics(
    struct scope_backend_diagnostics *diagnostics);
```

The first implementation may internally support only `scope_backend_capture_once()`. The stateful API is still defined now so the one-shot implementation naturally evolves into RUN/STOP/SINGLE operation.

## 5. Public types

### 5.1 Channels

```c
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
    SCOPE_CHANNEL_MASK_BOTH =
        SCOPE_CHANNEL_MASK_CH1 |
        SCOPE_CHANNEL_MASK_CH2
};
```

### 5.2 Coupling and trigger

```c
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
```

Additional modes are added only after the DPOS350P backend can prove them with hardware tests.

### 5.3 Capture status

```c
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
```

## 6. Channel configuration

```c
struct scope_channel_config {
    bool enabled;

    enum scope_coupling coupling;
    bool bandwidth_limit_enabled;

    uint32_t volts_per_div_uv;
    int32_t vertical_position_uv;

    uint32_t probe_ratio_numerator;
    uint32_t probe_ratio_denominator;
};
```

Example CH1 configuration:

```c
struct scope_channel_config ch1 = {
    .enabled = true,
    .coupling = SCOPE_COUPLING_DC,
    .bandwidth_limit_enabled = false,
    .volts_per_div_uv = 1000000U,
    .vertical_position_uv = 0,
    .probe_ratio_numerator = 1U,
    .probe_ratio_denominator = 1U
};
```

The DPOS350P backend is responsible for translating this configuration into:

```text
channel enable state
analog/frontend range
coupling command
bandwidth command
vertical-position payload
calibration selection inputs
trigger-level conversion context
```

The scope application must not calculate FPGA payloads.

## 7. Timebase configuration

```c
enum scope_timebase_id {
    SCOPE_TIMEBASE_10_NS,
    SCOPE_TIMEBASE_20_NS,
    SCOPE_TIMEBASE_50_NS,
    SCOPE_TIMEBASE_100_NS,
    SCOPE_TIMEBASE_200_NS,
    SCOPE_TIMEBASE_500_NS,
    SCOPE_TIMEBASE_1_US,
    SCOPE_TIMEBASE_2_US,
    SCOPE_TIMEBASE_5_US,
    SCOPE_TIMEBASE_10_US,
    SCOPE_TIMEBASE_20_US,
    SCOPE_TIMEBASE_50_US,
    SCOPE_TIMEBASE_100_US,
    SCOPE_TIMEBASE_200_US,
    SCOPE_TIMEBASE_500_US,
    SCOPE_TIMEBASE_1_MS,
    SCOPE_TIMEBASE_2_MS,
    SCOPE_TIMEBASE_5_MS,
    SCOPE_TIMEBASE_10_MS,
    SCOPE_TIMEBASE_20_MS,
    SCOPE_TIMEBASE_50_MS,
    SCOPE_TIMEBASE_100_MS,
    SCOPE_TIMEBASE_COUNT
};
```

This enumeration describes the desired UI contract. A backend capability table declares which entries are actually supported.

```c
struct scope_timebase_config {
    enum scope_timebase_id id;
    uint32_t requested_sample_count;
    uint16_t horizontal_position_per_mille;
};
```

The public API uses a stable timebase ID. The backend publishes the actual sample period in the result.

## 8. Trigger configuration

```c
struct scope_trigger_config {
    enum scope_trigger_mode mode;
    enum scope_channel source;
    enum scope_trigger_edge edge;

    int32_t level_uv;
    uint16_t horizontal_position_per_mille;

    uint32_t auto_timeout_ms;
};
```

The backend must convert `level_uv` to a hardware trigger code using the active frontend range, vertical position, probe ratio, and calibration state.

The scope application must not perform this conversion.

## 9. Capture request

```c
enum scope_capture_flags {
    SCOPE_CAPTURE_FLAG_NONE = 0U,
    SCOPE_CAPTURE_FLAG_RAW_DIAGNOSTIC = 1U << 0,
    SCOPE_CAPTURE_FLAG_NO_CALIBRATION = 1U << 1,
    SCOPE_CAPTURE_FLAG_SINGLE_SLOT = 1U << 2
};

struct scope_capture_request {
    struct scope_channel_config channel[SCOPE_CHANNEL_COUNT];
    struct scope_timebase_config timebase;
    struct scope_trigger_config trigger;

    uint32_t flags;
};
```

Diagnostic flags are primarily for tests. Normal scope operation should use the default calibrated path.

## 10. Waveform contract

```c
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
```

The backend owns the hardware capture and conversion process. The caller owns the destination sample buffers and sets `samples` and `capacity` before capture.

A successful result must define:

```text
valid sample count
sample ordering
sample period
zero code
microvolts per code
trigger index
active channel mask
```

## 11. Backend capabilities

```c
struct scope_timebase_info {
    enum scope_timebase_id id;
    uint64_t nominal_femtoseconds_per_division;
    uint32_t maximum_samples;
    uint32_t supported_channel_masks;
};

struct scope_range_info {
    uint32_t volts_per_div_uv;
    uint32_t supported_probe_ratios;
};

struct scope_backend_info {
    uint32_t channel_count;
    uint32_t maximum_samples;

    bool fpga_present;
    bool csi_present;
    bool calibration_loaded;

    const struct scope_timebase_info *timebases;
    uint32_t timebase_count;

    const struct scope_range_info *channel_ranges[SCOPE_CHANNEL_COUNT];
    uint32_t channel_range_count[SCOPE_CHANNEL_COUNT];
};
```

The first backend version may advertise only:

```text
CH1 enabled
CH2 disabled
one confirmed fixed profile
one confirmed frontend range
one basic trigger configuration
one calibrated output path
```

## 12. Backend state machine

```c
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
```

Allowed transitions:

```text
UNINITIALIZED -> IDLE
IDLE          -> CONFIGURED
CONFIGURED    -> ARMED
ARMED         -> WAITING_READY
WAITING_READY -> CAPTURING | ERROR
CAPTURING     -> PROCESSING | ERROR
PROCESSING    -> RESULT_READY | ERROR
RESULT_READY  -> ARMED | IDLE
ERROR         -> IDLE after explicit recovery/reset
```

The state machine prevents invalid sequences such as:

```text
capture without rearm
rearm while CSI owns the PE pins
profile changes during capture
FPGA register access while PE is in CSI sample mode
```

## 13. Private DPOS350P acquisition state

Proposed private type:

```c
struct dpos_acquisition_state {
    uint8_t raw_lane_mode;

    uint32_t capture_period;
    uint16_t stream_geometry_base;
    uint16_t runtime_position_base;

    uint16_t fpga_position;
    uint8_t capture_position_valid;
    uint32_t normalized_position;

    uint16_t runtime_step;
    uint16_t runtime_coarse_step;
    uint32_t runtime_fine_progress;
    uint32_t runtime_coarse_progress;
    uint32_t runtime_progress_threshold;

    uint8_t raw_lane_source_offset[8];
    uint8_t raw_lane_slot[8];

    uint8_t calibration_selector_base;
    uint8_t calibration_selector_limit;

    uint8_t horizontal_profile_04;
    uint8_t horizontal_profile_05;
    uint8_t horizontal_profile_06;
    uint8_t horizontal_mode_02;
    uint32_t horizontal_count_0d;
};
```

The member names follow the canonical HACK `SYS_INFO.md` names. This structure is independent from stock RAM and must not rely on `SYS_INFO` being present at `0x40544E68`.

## 14. Private DPOS350P profile

```c
struct dpos_acquisition_profile {
    enum scope_timebase_id timebase_id;
    uint8_t channel_mask;

    uint8_t raw_lane_mode;
    uint8_t active_lane_count;

    uint8_t horizontal_profile_04;
    uint8_t horizontal_profile_05;
    uint8_t horizontal_profile_06;
    uint8_t horizontal_mode_02;
    uint32_t horizontal_count_0d;

    uint32_t capture_period;
    uint16_t stream_geometry_base;
    uint16_t runtime_position_base;
    uint16_t fpga_stream_quantity;
    uint16_t csi_geometry;

    uint64_t sample_period_fs;
    uint32_t output_sample_count;
};
```

Confirmed geometry equations:

```c
csi_geometry = stream_geometry_base
             + 2U * runtime_position_base;

fpga_stream_quantity = 20U
                     + 2U * (stream_geometry_base
                           + runtime_position_base);
```

Confirmed fixed-profile values:

```text
stream_geometry_base  = 0x17FA
runtime_position_base = 0x0003
CSI geometry          = 0x1800
FPGA stream quantity  = 0x300E
```

These values are the first golden profile. They are not a general profile derivation algorithm.

## 15. Private raw-capture contract

```c
#define DPOS_RAW_SLOT_COUNT 8U
#define DPOS_RAW_SLOT_STRIDE 0x2134U
#define DPOS_CONFIRMED_ACTIVE_PAYLOAD 0x1800U

struct dpos_raw_capture {
    uint8_t *slot[DPOS_RAW_SLOT_COUNT];

    uint32_t slot_count;
    uint32_t active_bytes_per_slot;
    uint32_t captured_slot_mask;

    uint32_t capture_sequence;
};
```

The implementation may initially capture into a caller-provided contiguous buffer rather than the stock raw-ring address. The logical slot model must still be represented explicitly.

Raw lane addressing uses:

```c
raw_address = raw_slot_base
            + raw_lane_slot[lane] * raw_slot_stride
            + runtime_position_base
            + raw_lane_source_offset[lane];
```

The physical ADC/interleave interpretation of the lane arrays remains open.

## 16. Private calibration contract

```c
#define DPOS_CALIBRATION_COLUMN_COUNT 25U
#define DPOS_CALIBRATION_ROWS_PER_COLUMN 50U
#define DPOS_CALIBRATION_AUX_ARRAY_COUNT 4U
#define DPOS_CALIBRATION_AUX_ENTRY_COUNT 20U
#define DPOS_CALIBRATION_LANE_COUNT 8U

struct dpos_calibration {
    uint32_t matrix
        [DPOS_CALIBRATION_COLUMN_COUNT]
        [DPOS_CALIBRATION_ROWS_PER_COLUMN];

    uint32_t auxiliary
        [DPOS_CALIBRATION_AUX_ARRAY_COUNT]
        [DPOS_CALIBRATION_AUX_ENTRY_COUNT];

    bool valid;
};

struct dpos_active_calibration {
    uint32_t coefficient_a[DPOS_CALIBRATION_LANE_COUNT];
    uint32_t coefficient_b[DPOS_CALIBRATION_LANE_COUNT];

    uint32_t selector_delta;
    uint32_t valid_pair_count;
};
```

Proposed private API:

```c
int dpos_calibration_load(
    struct dpos_calibration *calibration);

int dpos_calibration_select(
    const struct dpos_calibration *calibration,
    const struct dpos_acquisition_state *state,
    struct dpos_active_calibration *active);
```

Confirmed persistence constraints:

```text
persistence source observed at 0x001FB000
transfer size                     0x5000
active serialized schema          0x14C8
scalar widths                      1, 2, 4, 8 bytes
multi-byte serialization           big-endian
```

The loader must be read-only. It must not call calibration save or factory-calibration generation.

## 17. Private implementation operations

The target logical operations are:

```c
int dpos_frontend_apply_channel(
    enum scope_channel channel,
    const struct scope_channel_config *config);

int dpos_profile_build(
    const struct scope_capture_request *request,
    struct dpos_acquisition_profile *profile);

int dpos_profile_apply(
    const struct dpos_acquisition_profile *profile);

int dpos_acquisition_prepare(
    const struct dpos_acquisition_profile *profile,
    struct dpos_acquisition_state *state);

int dpos_acquisition_rearm(
    struct dpos_acquisition_state *state);

int dpos_acquisition_capture_raw(
    struct dpos_acquisition_state *state,
    struct dpos_raw_capture *capture,
    uint32_t timeout_iterations);

int dpos_waveform_convert(
    const struct dpos_raw_capture *raw,
    const struct dpos_acquisition_state *state,
    const struct dpos_active_calibration *calibration,
    struct scope_capture_result *result);
```

These are logical boundaries, not a requirement to create all source files immediately.

The first implementation may remain in a small number of existing files while the vertical slice is proven. Refactoring should follow demonstrated ownership boundaries.

## 18. Intended capture flow

```c
scope_backend_capture_once(request, result, timeout_ms)
{
    validate_request(request);

    dpos_profile_build(request, &profile);

    dpos_frontend_apply_channel(CH1, &request->channel[0]);
    dpos_frontend_apply_channel(CH2, &request->channel[1]);
    dpos_profile_apply(&profile);

    dpos_acquisition_prepare(&profile, &state);
    dpos_acquisition_rearm(&state);
    dpos_acquisition_capture_raw(&state, &raw, timeout);

    dpos_calibration_select(&calibration, &state, &active);
    dpos_waveform_convert(&raw, &state, &active, result);

    return result->status;
}
```

The actual implementation must preserve the required PE GPIO/CSI ownership sequence and cache maintenance around DMA-visible buffers.

## 19. Diagnostics API

```c
struct scope_backend_diagnostics {
    enum scope_backend_state state;
    enum scope_capture_status last_status;

    uint8_t fpga_acquisition_status;
    uint8_t fpga_runtime_status;

    uint32_t csi_status_before;
    uint32_t csi_status_after;
    uint32_t csi_buffer0;
    uint32_t csi_geometry;

    uint16_t fpga_stream_quantity;
    uint16_t runtime_position_base;

    uint32_t ready_poll_count;
    uint32_t capture_poll_count;

    uint32_t raw_changed_bytes;
    uint32_t raw_nonzero_bytes;
    uint32_t raw_unique_values;
    uint32_t raw_hash;

    uint32_t calibration_selector_delta;
    uint32_t active_calibration_pairs;

    uint32_t first_error_stage;
};
```

Recommended error-stage identifiers:

```c
enum scope_backend_error_stage {
    SCOPE_ERROR_STAGE_NONE,
    SCOPE_ERROR_STAGE_VALIDATE,
    SCOPE_ERROR_STAGE_FRONTEND,
    SCOPE_ERROR_STAGE_PROFILE,
    SCOPE_ERROR_STAGE_REARM,
    SCOPE_ERROR_STAGE_READY,
    SCOPE_ERROR_STAGE_CSI_SETUP,
    SCOPE_ERROR_STAGE_CAPTURE,
    SCOPE_ERROR_STAGE_CACHE,
    SCOPE_ERROR_STAGE_CALIBRATION,
    SCOPE_ERROR_STAGE_CONVERSION
};
```

## 20. Acceptance contract

### 20.1 Transport acceptance

A raw capture passes only when all required conditions hold:

```text
FPGA initialization successful
profile application successful
runtime rearm successful
runtime READY observed within timeout
CSI destination and geometry are correct
CSI completion observed within timeout
cache invalidated before CPU inspection
expected active payload changed
payload is nonconstant
payload hash changes when the input signal changes
```

`FPGA READY` or `CSI DONE` alone is not sufficient.

### 20.2 Calibration acceptance

```text
persistence read succeeds
decoded schema length is 0x14C8
matrix sanity samples are plausible
selector produces 8/8 active pairs for the golden profile
```

### 20.3 Conversion acceptance

```text
single-sample custom output matches the D0.4W13 stock oracle for 8/8 lanes
full waveform is nonconstant
conversion stays inside source and destination bounds
sample order is explicitly defined
```

### 20.4 Measurement acceptance

Using a known external signal:

```text
frequency is within defined tolerance
period is within defined tolerance
Vpp is within defined tolerance
RMS is within defined tolerance
DC baseline responds correctly to offset and coupling
trigger index is stable for a stable periodic signal
```

## 21. Initial supported capability: M1

The first implementation target is deliberately narrow:

```text
CH1 enabled
CH2 disabled
one hardware-confirmed fixed profile
one confirmed frontend/range configuration
bounded one-shot capture
0x1800-byte raw payload
read-only calibration load
8-lane calibration selector
one calibrated uint16 CH1 waveform
full diagnostics snapshot
```

Unsupported requests return `SCOPE_CAPTURE_INVALID_CONFIG`.

## 22. Implementation milestones

### M0: preserve golden oracles

```text
DPOSLAB D0.4W13 PASS
standalone custom fixed-profile transport PASS
```

No refactoring may regress these results.

### M1: real raw-data parity

```text
stock runtime capture and custom capture use comparable profile state
custom payload is complete and nonconstant
known input changes custom raw payload predictably
```

### M2: custom calibration decode

```text
decoded cursor = 0x14C8
selected matrix samples match D0.4W13
no stock EXE2 function calls required
```

### M3: custom selector parity

```text
selector delta matches stock
8/8 active coefficient pairs match stock bit for bit
```

### M4: custom conversion parity

```text
8/8 single-lane sample results match stock
full waveform comparison has no unexplained differences
```

### M5: calibrated waveform API

```c
scope_backend_capture_once(request, result, timeout_ms);
```

returns a valid calibrated CH1 waveform and complete physical metadata for the golden profile.

### M6: minimal live scope

```text
one live CH1 waveform
RUN
STOP
SINGLE
one V/div
one timebase
one trigger source and edge
frequency and Vpp measurements
```

### M7: scope frontend integration

Adapt the suitable hardware-independent parts of the 1013D scope application to this API.

### M8: generalization

```text
multiple timebases
multiple V/div ranges
dual channel
complete trigger modes
continuous runtime capture
measurements
storage and screenshots
```

## 23. Relationship to the 1013D scope application

The 1013D project is a candidate source of architecture and, subject to its license, reusable application logic for:

```text
scope state machine
waveform rendering
measurements
menus and settings
cursors
RUN/STOP/SINGLE behavior
```

The following 1013D-specific areas must not be reused as the DPOS350P hardware backend:

```text
FPGA protocol
acquisition driver
raw buffer assumptions
platform display/touch drivers
boot and storage glue
hardware-specific calibration
```

The integration point is the public `scope_backend.h` contract, not any DPOS350P private type.

Direct reuse or adaptation of GPL-licensed 1013D code requires the resulting distributed work to comply with the applicable GPL terms. Architectural inspiration and independent implementation must remain genuinely independent rather than a mechanical rewrite.

## 24. Proposed source layout

This is a target layout, not an immediate refactoring requirement:

```text
docs/
    SCOPE_BACKEND.md

scope/
    scope_backend.h
    dpos_scope_backend.c

# Existing or future private implementation modules
drivers/
    fpga_bus.c
    fpga_protocol.c
    dpos_acq.c
    dpos_frontend.c
    dpos_profile.c
    dpos_calibration.c
    dpos_waveform.c
```

The first vertical slice may remain in existing source files. New modules should be created only when ownership is clear from working code.

## 25. Open design questions

The first implementation must track these explicitly:

```text
exact physical meaning of raw_lane_mode
physical ADC/interleave ordering of raw_lane_slot[8]
physical meaning of raw_lane_source_offset[8]
complete timebase-to-profile derivation
complete V/div-to-frontend derivation
trigger-level physical conversion
sample_period_fs derivation across profiles
microvolts_per_code derivation across ranges
CH1/CH2 separation in 4-slot and 8-slot modes
continuous runtime capture lifecycle
ownership of result buffers
```

Unresolved questions do not prevent the golden-profile implementation when the relevant values are fixed and documented as profile constants.

## 26. Review rule

A change to this API should answer at least one of these questions:

1. Does the scope application need this information?
2. Is the information generic to an oscilloscope rather than specific to DPOS350P hardware?
3. Can the backend guarantee the meaning for every advertised capability?
4. Does the change preserve the separation between HACK evidence and BOOT implementation?

If the answer is no, the information belongs in a private DPOS350P structure or in the HACK documentation, not in `scope_backend.h`.
