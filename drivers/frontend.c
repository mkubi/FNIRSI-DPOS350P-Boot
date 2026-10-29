#include <stdint.h>

#include "frontend.h"
#include "fpga_bus.h"
#include "fpga_protocol.h"
#include "timer.h"

#define FRONTEND_CH1_RANGE_SELECTOR       0x28U
#define FRONTEND_CH1_BANDWIDTH_SELECTOR   0x29U
#define FRONTEND_CH1_DIRECT_SELECTOR      0x2AU
#define FRONTEND_CH2_RANGE_SELECTOR       0x2BU
#define FRONTEND_CH2_BANDWIDTH_SELECTOR   0x2CU
#define FRONTEND_CH2_DIRECT_SELECTOR      0x2DU
#define FRONTEND_CHANNEL_SELECTOR         0x2EU
#define FRONTEND_CH1_COUPLING_SELECTOR    0x2FU
#define FRONTEND_CH2_COUPLING_SELECTOR    0x30U
#define FRONTEND_SHARED_SELECTOR          0x62U

#define FRONTEND_RANGE_200MV              0x06U
#define FRONTEND_BANDWIDTH_350MHZ         0x00U
#define FRONTEND_COUPLING_DEFAULT         0x00U
#define FRONTEND_CHANNEL_CH1_ON_CH2_OFF   0x01U

#define FRONTEND_CH1_JOB_08_VALUE         0x000012D9U
#define FRONTEND_CH1_JOB_0A_VALUE         0x00514901U
#define FRONTEND_CH1_COMMIT               0xB5U

#define FRONTEND_CH2_JOB_09_VALUE         0x000012D5U
#define FRONTEND_CH2_JOB_0B_VALUE         0x005125D0U
#define FRONTEND_CH2_COMMIT               0xB6U

#define FRONTEND_CHANNEL_SETTLE_MS        60U
#define FRONTEND_ROUTING_SETTLE_MS        50U

static void clear_result(struct frontend_init_result *result)
{
    result->completed_steps = 0U;
    result->failed_step = FRONTEND_STEP_NONE;
    result->last_job_result = 0U;
    result->last_job_attempts = 0U;
    result->last_job_error = 0;
    result->success = 0;
}

static void complete_step(
    struct frontend_init_result *result,
    enum frontend_step step)
{
    result->completed_steps |= 1U << ((uint32_t)step - 1U);
}

static int run_job(
    uint8_t command,
    uint32_t value,
    enum frontend_step step,
    struct frontend_init_result *result)
{
    struct fpga_protocol_transaction_result transaction;
    int ok;

    ok = fpga_protocol_parameter_transaction(
        command,
        value,
        &transaction);

    result->last_job_result = transaction.result66;
    result->last_job_attempts = transaction.attempts;
    result->last_job_error = transaction.error;

    if (!ok || !transaction.success) {
        result->failed_step = (uint32_t)step;
        return 0;
    }

    complete_step(result, step);
    return 1;
}

int frontend_init_stock_default(struct frontend_init_result *result)
{
    struct frontend_init_result local;
    struct frontend_init_result *out = result ? result : &local;

    clear_result(out);
    fpga_bus_control_idle();

    /* Shared stock frontend state. */
    fpga_protocol_write_u8(FRONTEND_SHARED_SELECTOR, 0x00U);
    complete_step(out, FRONTEND_STEP_SHARED);

    /* CH1: stock-confirmed 200 mV/div profile. */
    fpga_protocol_write_u8(
        FRONTEND_CH1_RANGE_SELECTOR,
        FRONTEND_RANGE_200MV);
    complete_step(out, FRONTEND_STEP_CH1_RANGE);

    if (!run_job(
            0x08U,
            FRONTEND_CH1_JOB_08_VALUE,
            FRONTEND_STEP_CH1_JOB_08,
            out))
        goto fail;

    if (!run_job(
            0x0AU,
            FRONTEND_CH1_JOB_0A_VALUE,
            FRONTEND_STEP_CH1_JOB_0A,
            out))
        goto fail;

    if (!run_job(
            FRONTEND_CH1_COMMIT,
            0x00000000U,
            FRONTEND_STEP_CH1_COMMIT,
            out))
        goto fail;

    fpga_protocol_write_u16_be(FRONTEND_CH1_DIRECT_SELECTOR, 0x0000U);
    complete_step(out, FRONTEND_STEP_CH1_DIRECT);
    timer_delay_ms(FRONTEND_CHANNEL_SETTLE_MS);

    /* CH2: stock-confirmed 200 mV/div stored profile. */
    fpga_protocol_write_u8(
        FRONTEND_CH2_RANGE_SELECTOR,
        FRONTEND_RANGE_200MV);
    complete_step(out, FRONTEND_STEP_CH2_RANGE);

    if (!run_job(
            0x09U,
            FRONTEND_CH2_JOB_09_VALUE,
            FRONTEND_STEP_CH2_JOB_09,
            out))
        goto fail;

    if (!run_job(
            0x0BU,
            FRONTEND_CH2_JOB_0B_VALUE,
            FRONTEND_STEP_CH2_JOB_0B,
            out))
        goto fail;

    if (!run_job(
            FRONTEND_CH2_COMMIT,
            0x00000000U,
            FRONTEND_STEP_CH2_COMMIT,
            out))
        goto fail;

    fpga_protocol_write_u16_be(FRONTEND_CH2_DIRECT_SELECTOR, 0x0000U);
    complete_step(out, FRONTEND_STEP_CH2_DIRECT);
    timer_delay_ms(FRONTEND_CHANNEL_SETTLE_MS);

    /* Coupling and per-channel bandwidth defaults. */
    fpga_protocol_write_u8(
        FRONTEND_CH1_COUPLING_SELECTOR,
        FRONTEND_COUPLING_DEFAULT);
    complete_step(out, FRONTEND_STEP_CH1_COUPLING);

    fpga_protocol_write_u8(
        FRONTEND_CH1_BANDWIDTH_SELECTOR,
        FRONTEND_BANDWIDTH_350MHZ);
    complete_step(out, FRONTEND_STEP_CH1_BANDWIDTH);

    fpga_protocol_write_u8(
        FRONTEND_CH2_COUPLING_SELECTOR,
        FRONTEND_COUPLING_DEFAULT);
    complete_step(out, FRONTEND_STEP_CH2_COUPLING);

    fpga_protocol_write_u8(
        FRONTEND_CH2_BANDWIDTH_SELECTOR,
        FRONTEND_BANDWIDTH_350MHZ);
    complete_step(out, FRONTEND_STEP_CH2_BANDWIDTH);

    /* Final stock default routing: CH1 enabled, CH2 disabled. */
    fpga_protocol_write_u8(
        FRONTEND_CHANNEL_SELECTOR,
        FRONTEND_CHANNEL_CH1_ON_CH2_OFF);
    complete_step(out, FRONTEND_STEP_CHANNEL_ROUTING);

    timer_delay_ms(FRONTEND_ROUTING_SETTLE_MS);
    fpga_bus_safe_idle();

    out->success = 1;
    return 1;

fail:
    fpga_bus_safe_idle();
    out->success = 0;
    return 0;
}
