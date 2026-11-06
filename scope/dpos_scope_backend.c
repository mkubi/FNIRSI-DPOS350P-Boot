#include <stddef.h>
#include <stdint.h>

#include "scope_backend.h"
#include "acq.h"
#include "cache.h"
#include "fpga_init.h"
#include "fpga_protocol.h"
#include "frontend.h"
#include "profile.h"
#include "r11_csi.h"
#include "smec.h"
#include "timer.h"

#define DPOS_GOLDEN_RAW_BYTES            0x1800U
#define DPOS_GOLDEN_STREAM_QUANTITY      0x300EU
#define DPOS_GOLDEN_RUNTIME_POSITION     0x0003U
#define DPOS_GOLDEN_MODE                 0x01U
#define DPOS_GOLDEN_READY_POLLS          0x00100000U
#define DPOS_CAPTURE_FILL                0xA5U
#define DPOS_DEFAULT_TIMEOUT_ITERATIONS  0x01000000U
#define DPOS_ITERATIONS_PER_MS           100000U
#define DPOS_FNV_OFFSET                  2166136261U
#define DPOS_FNV_PRIME                   16777619U

static uint8_t raw_buffer[DPOS_GOLDEN_RAW_BYTES] __attribute__((aligned(64)));
static struct scope_backend_diagnostics backend_diag;
static struct scope_capture_request configured_request;
static uint32_t backend_initialized;
static uint32_t request_configured;

static void zero_bytes(void *pointer, uint32_t size)
{
    uint8_t *p = (uint8_t *)pointer;
    while (size-- != 0U)
        *p++ = 0U;
}

static void set_error(
    enum scope_capture_status status,
    enum scope_backend_error_stage stage)
{
    backend_diag.state = SCOPE_BACKEND_ERROR;
    backend_diag.last_status = status;
    if (backend_diag.first_error_stage == SCOPE_ERROR_STAGE_NONE)
        backend_diag.first_error_stage = stage;
}

static int apply_factory_acquisition_geometry(void)
{
    static const struct {
        uint8_t reg;
        uint8_t length;
        uint32_t value;
    } registers[] = {
        { 0x01U, 1U, 0x00000014U },
        { 0x02U, 4U, 0x00000040U },
        { 0x03U, 4U, 0x00000040U },
        { 0x04U, 4U, 0x000003E8U },
        { 0x05U, 4U, 0x00000064U },
        { 0x06U, 2U, 0x000003B6U },
        { 0x07U, 4U, 0x000000C8U }
    };
    struct fpga_protocol_transaction_result transaction;
    uint32_t i;

    for (i = 0U; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        if (!fpga_protocol_register_write(
                registers[i].reg,
                registers[i].length,
                registers[i].value,
                &transaction))
            return 0;
    }
    return fpga_protocol_register_commit(0xB4U, &transaction);
}

static int apply_golden_trigger(void)
{
    static const struct {
        uint8_t command;
        uint32_t value;
    } jobs[] = {
        { 0x0EU, 0x00000020U },
        { 0x0FU, 0x000000FBU },
        { 0x10U, 0x000000FBU },
        { 0x11U, 0x000008F6U },
        { 0x12U, 0x00000FFFU },
        { 0x13U, 0x000067D0U },
        { 0xB8U, 0x00000000U }
    };
    struct fpga_protocol_transaction_result transaction;
    uint32_t i;

    fpga_protocol_write_u8(0x61U, 0x00U);
    fpga_protocol_write_u16_be(0x0CU, 0x0031U);
    fpga_protocol_write_u8(0x1AU, 0x00U);
    fpga_protocol_write_u8(0x15U, 0x02U);
    fpga_protocol_write_u8(0x16U, 0x00U);

    for (i = 0U; i < sizeof(jobs) / sizeof(jobs[0]); ++i) {
        if (!fpga_protocol_parameter_transaction(
                jobs[i].command, jobs[i].value, &transaction))
            return 0;
    }

    fpga_protocol_write_u16_be(0x1BU, 0x0BFDU);
    fpga_protocol_write_u16_be(0x1CU, 0x0BFDU);
    return 1;
}

static int request_is_golden(const struct scope_capture_request *request)
{
    uint32_t required_flags =
        SCOPE_CAPTURE_FLAG_RAW_DIAGNOSTIC |
        SCOPE_CAPTURE_FLAG_NO_CALIBRATION |
        SCOPE_CAPTURE_FLAG_SINGLE_SLOT;

    if (request == NULL)
        return 0;
    if (!request->channel[SCOPE_CHANNEL_1].enabled ||
        request->channel[SCOPE_CHANNEL_2].enabled)
        return 0;
    if (request->channel[SCOPE_CHANNEL_1].coupling != SCOPE_COUPLING_DC)
        return 0;
    if (request->channel[SCOPE_CHANNEL_1].volts_per_div_uv != 200000U)
        return 0;
    if (request->channel[SCOPE_CHANNEL_1].probe_ratio_numerator != 1U ||
        request->channel[SCOPE_CHANNEL_1].probe_ratio_denominator != 1U)
        return 0;
    if (request->timebase.id != SCOPE_TIMEBASE_20_US ||
        request->timebase.requested_sample_count != DPOS_GOLDEN_RAW_BYTES)
        return 0;
    if (request->trigger.source != SCOPE_CHANNEL_1 ||
        request->trigger.edge != SCOPE_TRIGGER_RISING)
        return 0;
    if ((request->flags & required_flags) != required_flags)
        return 0;
    return 1;
}

static void result_reset(struct scope_capture_result *result)
{
    uint32_t channel;
    if (result == NULL)
        return;
    result->status = SCOPE_CAPTURE_INTERNAL_ERROR;
    result->active_channel_mask = SCOPE_CHANNEL_MASK_CH1;
    result->trigger_index = 0U;
    result->capture_sequence = backend_diag.capture_sequence;
    result->diagnostic_flags = 0U;
    for (channel = 0U; channel < SCOPE_CHANNEL_COUNT; ++channel) {
        result->channel[channel].sample_count = 0U;
        result->channel[channel].sample_period_fs = 0U;
        result->channel[channel].zero_code = 0U;
        result->channel[channel].microvolts_per_code = 0;
        result->channel[channel].minimum = 0U;
        result->channel[channel].maximum = 0U;
        result->channel[channel].average = 0U;
        result->channel[channel].valid = false;
    }
}

static void fill_raw(uint8_t value)
{
    uint32_t i;
    for (i = 0U; i < DPOS_GOLDEN_RAW_BYTES; ++i)
        raw_buffer[i] = value;
}

static void analyze_raw(uint8_t original)
{
    uint32_t histogram[256];
    uint32_t i;
    uint32_t hash = DPOS_FNV_OFFSET;
    uint32_t changed = 0U;
    uint32_t nonzero = 0U;
    uint32_t unique = 0U;
    uint32_t first = 0xFFFFFFFFU;
    uint32_t last = 0xFFFFFFFFU;
    uint8_t minimum = 0xFFU;
    uint8_t maximum = 0x00U;

    zero_bytes(histogram, sizeof(histogram));
    for (i = 0U; i < DPOS_GOLDEN_RAW_BYTES; ++i) {
        uint8_t value = raw_buffer[i];
        if (value != original) {
            if (changed == 0U)
                first = i;
            last = i;
            ++changed;
        }
        if (value != 0U)
            ++nonzero;
        if (value < minimum)
            minimum = value;
        if (value > maximum)
            maximum = value;
        histogram[value] += 1U;
        hash ^= value;
        hash *= DPOS_FNV_PRIME;
    }
    for (i = 0U; i < 256U; ++i)
        if (histogram[i] != 0U)
            ++unique;

    backend_diag.raw_changed_bytes = changed;
    backend_diag.raw_nonzero_bytes = nonzero;
    backend_diag.raw_unique_values = unique;
    backend_diag.raw_hash = hash;
    backend_diag.raw_first_changed = first;
    backend_diag.raw_last_changed = last;
    backend_diag.raw_minimum = minimum;
    backend_diag.raw_maximum = maximum;
}

int scope_backend_init(struct scope_backend_info *info)
{
    struct smec_init_result smec;
    struct profile_init_result profile;
    struct frontend_init_result frontend;

    zero_bytes(&backend_diag, sizeof(backend_diag));
    backend_diag.state = SCOPE_BACKEND_UNINITIALIZED;
    backend_diag.last_status = SCOPE_CAPTURE_INTERNAL_ERROR;
    backend_diag.fpga_stream_quantity = DPOS_GOLDEN_STREAM_QUANTITY;
    backend_diag.runtime_position_base = DPOS_GOLDEN_RUNTIME_POSITION;
    backend_diag.csi_geometry = DPOS_GOLDEN_RAW_BYTES;
    backend_initialized = 0U;
    request_configured = 0U;

    if (!fpga_init(NULL)) {
        set_error(SCOPE_CAPTURE_FPGA_ERROR, SCOPE_ERROR_STAGE_FPGA_INIT);
        return 0;
    }
    if (!smec_init(&smec)) {
        set_error(SCOPE_CAPTURE_FPGA_ERROR, SCOPE_ERROR_STAGE_SMEC);
        return 0;
    }
    if (!apply_factory_acquisition_geometry()) {
        set_error(SCOPE_CAPTURE_FPGA_ERROR, SCOPE_ERROR_STAGE_GEOMETRY);
        return 0;
    }
    if (!profile_init(&profile, NULL)) {
        set_error(SCOPE_CAPTURE_FPGA_ERROR, SCOPE_ERROR_STAGE_PROFILE);
        return 0;
    }
    if (!frontend_init_stock_default(&frontend)) {
        set_error(SCOPE_CAPTURE_FPGA_ERROR, SCOPE_ERROR_STAGE_FRONTEND);
        return 0;
    }

    acq_configure_stream();
    backend_initialized = 1U;
    backend_diag.state = SCOPE_BACKEND_IDLE;
    backend_diag.last_status = SCOPE_CAPTURE_OK;

    if (info != NULL) {
        info->channel_count = 2U;
        info->maximum_samples = DPOS_GOLDEN_RAW_BYTES;
        info->fpga_present = true;
        info->csi_present = true;
        info->calibration_loaded = false;
        info->supported_channel_masks = SCOPE_CHANNEL_MASK_CH1;
        info->supported_timebase_mask = 1U << SCOPE_TIMEBASE_20_US;
    }
    return 1;
}

void scope_backend_deinit(void)
{
    scope_backend_stop();
    backend_initialized = 0U;
    request_configured = 0U;
    backend_diag.state = SCOPE_BACKEND_UNINITIALIZED;
}

int scope_backend_configure(const struct scope_capture_request *request)
{
    if (!backend_initialized || !request_is_golden(request)) {
        set_error(SCOPE_CAPTURE_INVALID_CONFIG, SCOPE_ERROR_STAGE_VALIDATE);
        return 0;
    }
    configured_request = *request;
    request_configured = 1U;
    backend_diag.state = SCOPE_BACKEND_CONFIGURED;
    backend_diag.last_status = SCOPE_CAPTURE_OK;
    backend_diag.first_error_stage = SCOPE_ERROR_STAGE_NONE;
    return 1;
}

int scope_backend_start(void)
{
    if (!backend_initialized || !request_configured)
        return 0;
    backend_diag.state = SCOPE_BACKEND_ARMED;
    return 1;
}

enum scope_capture_status scope_backend_poll(struct scope_capture_result *result)
{
    (void)result;
    return SCOPE_CAPTURE_WAITING;
}

int scope_backend_stop(void)
{
    r11_csi_capture_disable();
    backend_diag.state = backend_initialized ? SCOPE_BACKEND_IDLE
                                             : SCOPE_BACKEND_UNINITIALIZED;
    return 1;
}

enum scope_capture_status scope_backend_capture_once(
    const struct scope_capture_request *request,
    struct scope_capture_result *result,
    uint32_t timeout_ms)
{
    struct acq_prepare_result prepare;
    struct acq_capture_result capture;
    uint32_t timeout_iterations;

    result_reset(result);
    if (!scope_backend_configure(request))
        goto fail;
    if (!apply_golden_trigger()) {
        set_error(SCOPE_CAPTURE_FPGA_ERROR, SCOPE_ERROR_STAGE_TRIGGER);
        goto fail;
    }

    backend_diag.state = SCOPE_BACKEND_WAITING_READY;
    if (!acq_prepare(DPOS_GOLDEN_MODE, DPOS_GOLDEN_READY_POLLS, &prepare)) {
        backend_diag.fpga_acquisition_status_before = prepare.status_before;
        backend_diag.fpga_acquisition_status_after = prepare.status_after;
        backend_diag.ready_poll_count = prepare.polls;
        set_error(SCOPE_CAPTURE_TIMEOUT, SCOPE_ERROR_STAGE_READY);
        goto fail;
    }

    backend_diag.fpga_acquisition_status_before = prepare.status_before;
    backend_diag.fpga_acquisition_status_after = prepare.status_after;
    backend_diag.ready_poll_count = prepare.polls;
    backend_diag.prepared_capture_position = prepare.capture_position;
    backend_diag.live_capture_pointer = prepare.live_pointer;

    fill_raw(DPOS_CAPTURE_FILL);
    cache_clean_range((uintptr_t)raw_buffer, sizeof(raw_buffer));
    cache_dsb();

    timeout_iterations = timeout_ms == 0U
        ? DPOS_DEFAULT_TIMEOUT_ITERATIONS
        : timeout_ms * DPOS_ITERATIONS_PER_MS;
    if (timeout_iterations < DPOS_ITERATIONS_PER_MS)
        timeout_iterations = DPOS_ITERATIONS_PER_MS;

    backend_diag.state = SCOPE_BACKEND_CAPTURING;
    if (!acq_capture(
            (uintptr_t)raw_buffer,
            prepare.capture_position,
            0U,
            timeout_iterations,
            &capture)) {
        backend_diag.csi_status_before = capture.status_before;
        backend_diag.csi_status_after = capture.status_after;
        backend_diag.csi_buffer0 = capture.buffer0;
        backend_diag.capture_poll_count = capture.wait_polls;
        set_error(SCOPE_CAPTURE_TIMEOUT, SCOPE_ERROR_STAGE_CAPTURE);
        goto fail;
    }

    cache_invalidate_range((uintptr_t)raw_buffer, sizeof(raw_buffer));
    cache_dsb();

    backend_diag.state = SCOPE_BACKEND_PROCESSING;
    backend_diag.csi_status_before = capture.status_before;
    backend_diag.csi_status_after = capture.status_after;
    backend_diag.csi_buffer0 = capture.buffer0;
    backend_diag.capture_poll_count = capture.wait_polls;
    backend_diag.fpga_runtime_status = fpga_protocol_read_u8(0x0BU);
    analyze_raw(DPOS_CAPTURE_FILL);
    backend_diag.capture_sequence += 1U;

    if (backend_diag.raw_changed_bytes != DPOS_GOLDEN_RAW_BYTES) {
        set_error(SCOPE_CAPTURE_CSI_ERROR, SCOPE_ERROR_STAGE_CAPTURE);
        goto fail;
    }

    backend_diag.state = SCOPE_BACKEND_RESULT_READY;
    backend_diag.last_status = SCOPE_CAPTURE_OK;
    if (result != NULL) {
        result->status = SCOPE_CAPTURE_OK;
        result->capture_sequence = backend_diag.capture_sequence;
        result->active_channel_mask = SCOPE_CHANNEL_MASK_CH1;
        result->diagnostic_flags = request->flags;
        /* No calibrated/ordered waveform is claimed in M1A. */
        result->channel[SCOPE_CHANNEL_1].valid = false;
    }
    return SCOPE_CAPTURE_OK;

fail:
    if (result != NULL) {
        result->status = backend_diag.last_status;
        result->capture_sequence = backend_diag.capture_sequence;
    }
    return backend_diag.last_status;
}

void scope_backend_get_diagnostics(struct scope_backend_diagnostics *diagnostics)
{
    const uint32_t *source;
    uint32_t *destination;
    uint32_t i;

    if (diagnostics == NULL)
        return;

    source = (const uint32_t *)(const void *)&backend_diag;
    destination = (uint32_t *)(void *)diagnostics;

    for (i = 0U;
         i < sizeof(*diagnostics) / sizeof(uint32_t);
         ++i) {
        destination[i] = source[i];
    }
}

const uint8_t *scope_backend_raw_data(uint32_t *size)
{
    if (size != NULL)
        *size = DPOS_GOLDEN_RAW_BYTES;
    return raw_buffer;
}
