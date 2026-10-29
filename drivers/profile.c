#include "profile.h"
#include "fpga_protocol.h"
#include "timer.h"

#define PROFILE_STATE_BASE 0x40544E68U

/* Diagnostic captures retained privately while extracting the proven path. */
struct profile_sc1_5_1_capture {
    uint32_t magic, divisor_1b1, source_524, mode_588, scale_01c;
    uint32_t out_6e4_6e7, out_6e8;
};
struct profile_sc1_5_3_capture { uint32_t magic, mode_1b0, key_538, rate_53c; };
static volatile struct profile_sc1_5_1_capture g_dposlab_sc1_5_1_capture;
static volatile struct profile_sc1_5_3_capture g_dposlab_sc1_5_3_capture;
#define DPOSLAB_SC1_5_1_CAPTURE_MAGIC 0x53313531U
#define DPOSLAB_SC1_5_3_CAPTURE_MAGIC 0x53313533U
static volatile uint32_t profile_sc1_3_error;
static volatile uint32_t profile_sc1_5_1_error;
static volatile uint32_t profile_sc1_5_3_error;
static profile_progress_fn active_progress;
static uint32_t active_completed_steps;

static void report_progress(uint32_t operation)
{
    if (active_progress)
        active_progress(active_completed_steps, operation);
}

static void profile_seed_confirmed_inputs(void)
{
    volatile uint8_t *state8 = (volatile uint8_t *)(uintptr_t)PROFILE_STATE_BASE;
    volatile uint16_t *scale = (volatile uint16_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x1CU);
    volatile uint32_t *state32 = (volatile uint32_t *)(uintptr_t)PROFILE_STATE_BASE;

    /* Inputs formerly inherited from the stock startup environment. */
    state8[0x3EU] = 1U;
    state8[0x102U] = 0U;
    state8[0x588U] = 0x17U;
    state32[0x524U / 4U] = 8U;
    state32[0x554U / 4U] = 0x00002710U;
    *scale = 0x005FU;
    __asm__ volatile ("dmb sy" ::: "memory");
}

struct horizontal_h2_word {
    uint16_t offset;
    uint16_t reserved;
    uint32_t value;
};

/*
 * Exact word-level reproduction of stock 0x4003A680.
 *
 * This function only initializes the horizontal/timebase table in the stock
 * application state. It performs no FPGA, CSI or interrupt I/O.
 */
static void timebase_table_init(void)
{
    static const struct horizontal_h2_word words[] = {
        { 0x0BD0U, 0U, 0x00000001U },
        { 0x0BD4U, 0U, 0x00000000U },
        { 0x0BD8U, 0U, 0x00000002U },
        { 0x0BDCU, 0U, 0x00000000U },
        { 0x0BE0U, 0U, 0x00000005U },
        { 0x0BE4U, 0U, 0x00000000U },
        { 0x0BE8U, 0U, 0x0000000AU },
        { 0x0BECU, 0U, 0x00000000U },
        { 0x0BF0U, 0U, 0x00000014U },
        { 0x0BF4U, 0U, 0x00000000U },
        { 0x0BF8U, 0U, 0x00000032U },
        { 0x0BFCU, 0U, 0x00000000U },
        { 0x0C00U, 0U, 0x00000064U },
        { 0x0C04U, 0U, 0x00000000U },
        { 0x0C08U, 0U, 0x000000C8U },
        { 0x0C0CU, 0U, 0x00000000U },
        { 0x0C10U, 0U, 0x000001F4U },
        { 0x0C14U, 0U, 0x00000000U },
        { 0x0C18U, 0U, 0x000003E8U },
        { 0x0C1CU, 0U, 0x00000000U },
        { 0x0C20U, 0U, 0x000007D0U },
        { 0x0C24U, 0U, 0x00000000U },
        { 0x0C28U, 0U, 0x004C4B40U },
        { 0x0C2CU, 0U, 0x00000000U },
        { 0x0C30U, 0U, 0x00001388U },
        { 0x0C34U, 0U, 0x00000000U },
        { 0x0C38U, 0U, 0x00002710U },
        { 0x0C3CU, 0U, 0x00000000U },
        { 0x0C40U, 0U, 0x00004E20U },
        { 0x0C44U, 0U, 0x00000000U },
        { 0x0C48U, 0U, 0x0000C350U },
        { 0x0C4CU, 0U, 0x00000000U },
        { 0x0C50U, 0U, 0x000186A0U },
        { 0x0C54U, 0U, 0x00000000U },
        { 0x0C58U, 0U, 0x00030D40U },
        { 0x0C5CU, 0U, 0x00000000U },
        { 0x0C60U, 0U, 0x0007A120U },
        { 0x0C64U, 0U, 0x00000000U },
        { 0x0C68U, 0U, 0x000F4240U },
        { 0x0C6CU, 0U, 0x00000000U },
        { 0x0C70U, 0U, 0x001E8480U },
        { 0x0C74U, 0U, 0x00000000U },
        { 0x0C78U, 0U, 0x004C4B40U },
        { 0x0C7CU, 0U, 0x00000000U },
        { 0x0C80U, 0U, 0x00989680U },
        { 0x0C84U, 0U, 0x00000000U },
        { 0x0C88U, 0U, 0x01312D00U },
        { 0x0C8CU, 0U, 0x00000000U },
        { 0x0C90U, 0U, 0x02FAF080U },
        { 0x0C94U, 0U, 0x00000000U },
        { 0x0C98U, 0U, 0x05F5E100U },
        { 0x0C9CU, 0U, 0x00000000U },
        { 0x0CA0U, 0U, 0x0BEBC200U },
        { 0x0CA4U, 0U, 0x00000000U },
        { 0x0CA8U, 0U, 0x1DCD6500U },
        { 0x0CACU, 0U, 0x00000000U },
        { 0x0CB0U, 0U, 0x3B9ACA00U },
        { 0x0CB4U, 0U, 0x00000000U },
        { 0x0CB8U, 0U, 0x77359400U },
        { 0x0CBCU, 0U, 0x00000000U },
        { 0x0CC0U, 0U, 0x2A05F200U },
        { 0x0CC4U, 0U, 0x00000001U },
        { 0x0CC8U, 0U, 0x540BE400U },
        { 0x0CCCU, 0U, 0x00000002U },
        { 0x0CD0U, 0U, 0xA817C800U },
        { 0x0CD4U, 0U, 0x00000004U },
        { 0x0CD8U, 0U, 0xA43B7400U },
        { 0x0CDCU, 0U, 0x0000000BU },
        { 0x0CE0U, 0U, 0x4876E800U },
        { 0x0CE4U, 0U, 0x00000017U },
        { 0x0CE8U, 0U, 0x90EDD000U },
        { 0x0CECU, 0U, 0x0000002EU },
        { 0x0CF0U, 0U, 0x6A528800U },
        { 0x0CF4U, 0U, 0x00000074U },
        { 0x0CF8U, 0U, 0xD4A51000U },
        { 0x0CFCU, 0U, 0x000000E8U },
        { 0x0D00U, 0U, 0xA94A2000U },
        { 0x0D04U, 0U, 0x000001D1U },
        { 0x0D08U, 0U, 0x27395000U },
        { 0x0D0CU, 0U, 0x0000048CU },
        { 0x0D10U, 0U, 0x4E72A000U },
        { 0x0D14U, 0U, 0x00000918U },
        { 0x0D18U, 0U, 0x9CE54000U },
        { 0x0D1CU, 0U, 0x00001230U },
        { 0x0D20U, 0U, 0x883D2000U },
        { 0x0D24U, 0U, 0x00002D79U },
    };
    uint32_t i;

    for (i = 0U; i < sizeof(words) / sizeof(words[0]); ++i) {
        *(volatile uint32_t *)(uintptr_t)
            (PROFILE_STATE_BASE + words[i].offset) = words[i].value;
    }

    __asm__ volatile ("dmb sy" ::: "memory");
}

static void profile_mode_init(void)
{
    volatile uint8_t *state8 =
        (volatile uint8_t *)(uintptr_t)PROFILE_STATE_BASE;
    volatile uint32_t *state32 =
        (volatile uint32_t *)(uintptr_t)PROFILE_STATE_BASE;
    uint32_t mode;
    uint32_t divisor;
    uint32_t flag;

    if (state8[0x3EU] != 0U) {
        if (state8[0x102U] != 0U) {
            mode = 0U;
            divisor = 2U;
            flag = 1U;
        } else {
            mode = 1U;
            divisor = 1U;
            flag = 0U;
        }
    } else {
        mode = 2U;
        divisor = 1U;
        flag = 0U;
    }

    state8[0x100U] = (uint8_t)mode;
    state8[0x1B0U] = (uint8_t)divisor;
    state8[0x1ACU] = (uint8_t)flag;
    state8[0x1B1U] = (uint8_t)(state32[0x524U / 4U] / divisor);

    __asm__ volatile ("dmb sy" ::: "memory");
}

static void fpga_profile_apply_byte(volatile uint8_t *state,
                                     uint32_t source_offset,
                                     uint32_t shadow_offset,
                                     uint32_t parameter,
                                     uint32_t commit,
                                     uint32_t selector,
                                     uint32_t *changed)
{
    uint32_t byte_value = state[source_offset];

    if (state[shadow_offset] == (uint8_t)byte_value)
        return;

    state[shadow_offset] = (uint8_t)byte_value;
    report_progress(0x00000100U | parameter);
    fpga_protocol_parameter_transaction(
        (uint8_t)parameter,
        byte_value,
        0);
    report_progress(0x00000100U | commit);
    fpga_protocol_parameter_transaction(
        (uint8_t)commit,
        0U,
        0);
    report_progress(0x00000200U | selector);
    fpga_protocol_write_u8((uint8_t)selector, (uint8_t)byte_value);
    *changed = 1U;
}

static void fpga_profile_apply(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)PROFILE_STATE_BASE;
    volatile uint32_t *source_word =
        (volatile uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x6E8U);
    volatile uint32_t *shadow_word =
        (volatile uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0xBB4U);
    uint32_t changed = 0U;
    uint32_t value;

    fpga_profile_apply_byte(state, 0x6E4U, 0xBB0U,
                              0x21U, 0xC1U, 0x04U, &changed);
    fpga_profile_apply_byte(state, 0x6E5U, 0xBB1U,
                              0x22U, 0xC2U, 0x05U, &changed);
    fpga_profile_apply_byte(state, 0x6E6U, 0xBB2U,
                              0x23U, 0xC3U, 0x06U, &changed);

    value = state[0x6E7U];
    if (state[0xBB3U] != (uint8_t)value) {
        state[0xBB3U] = (uint8_t)value;
        report_progress(PROFILE_PROGRESS_SELECT_02);
        fpga_protocol_write_u8(0x02U, (uint8_t)value);
        changed = 1U;
    }

    value = *source_word;
    if (*shadow_word != value) {
        *shadow_word = value;
        report_progress(PROFILE_PROGRESS_SELECT_0D);
        fpga_protocol_write_u32_be(0x0DU, value);
        changed = 1U;
    }

    if (changed != 0U) {
        report_progress(PROFILE_PROGRESS_PULSE_07);
        fpga_protocol_write_u8(0x07U, 1U);
        fpga_protocol_write_u8(0x07U, 0U);
        report_progress(PROFILE_PROGRESS_AVS_DELAY);
        timer_avs_delay_units(2U);
    }
}

static void channel_calibration_apply(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)PROFILE_STATE_BASE;
    uint32_t i;

    for (i = 0U; i < 8U; ++i) {
        uint32_t source_offset = 0x674U + i;
        uint32_t shadow_offset = 0xBB8U + i;
        uint32_t byte_value = state[source_offset];

        if (state[shadow_offset] == (uint8_t)byte_value)
            continue;

        state[shadow_offset] = (uint8_t)byte_value;

        fpga_protocol_parameter_transaction(
            (uint8_t)(0x19U + i),
            byte_value,
            0);
        fpga_protocol_parameter_transaction(
            (uint8_t)(0xB9U + i),
            0U,
            0);

        fpga_protocol_write_u8(
            (uint8_t)(0xA6U + i),
            (uint8_t)byte_value);
    }

    /* Stock SC1.5.4 always emits the final selector 0x11 pulse. */
    fpga_protocol_write_u8(0x11U, 0U);
    fpga_protocol_write_u8(0x11U, 1U);
}

static void channel_order_init(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)PROFILE_STATE_BASE;
    volatile const uint32_t *key =
        (volatile const uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x538U);
    volatile const uint32_t *rate =
        (volatile const uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x53CU);
    uint32_t i;

    g_dposlab_sc1_5_3_capture.magic = DPOSLAB_SC1_5_3_CAPTURE_MAGIC;
    g_dposlab_sc1_5_3_capture.mode_1b0 = state[0x1B0U];
    g_dposlab_sc1_5_3_capture.key_538 = *key;
    g_dposlab_sc1_5_3_capture.rate_53c = *rate;

    if (state[0x1B0U] != 1U ||
        *key != 0x4C1A6700U ||
        *rate != 0x00000037U) {
        profile_sc1_5_3_error = 1U;
        __asm__ volatile ("dmb sy" ::: "memory");
        return;
    }

    profile_sc1_5_3_error = 0U;

    state[0x674U] = 0xF0U;
    state[0x675U] = 0x1EU;
    state[0x676U] = 0x0FU;
    state[0x677U] = 0xE1U;
    state[0x678U] = 0x78U;
    state[0x679U] = 0x3CU;
    state[0x67AU] = 0x87U;
    state[0x67BU] = 0xC3U;

    for (i = 0U; i < 8U; ++i)
        state[0x67CU + i] = state[0x69CU + i];

    *(volatile uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x6D4U) = 0U;
    *(volatile uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x6D8U) = 0U;

    state[0x6DCU] = 2U;
    state[0x6DDU] = 6U;
    state[0x6DEU] = 7U;
    state[0x6DFU] = 3U;
    state[0x6E0U] = 0U;
    state[0x6E1U] = 4U;
    state[0x6E2U] = 5U;
    state[0x6E3U] = 1U;

    __asm__ volatile ("dmb sy" ::: "memory");
}

static void acquisition_layout_init(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)PROFILE_STATE_BASE;
    volatile const uint16_t *scale =
        (volatile const uint16_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x1CU);
    volatile const uint32_t *source =
        (volatile const uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x524U);
    volatile const uint32_t *key_lo =
        (volatile const uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x538U);
    volatile const uint32_t *key_hi =
        (volatile const uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x53CU);

    g_dposlab_sc1_5_1_capture.magic = DPOSLAB_SC1_5_1_CAPTURE_MAGIC;
    g_dposlab_sc1_5_1_capture.divisor_1b1 = state[0x1B1U];
    g_dposlab_sc1_5_1_capture.source_524 = *source;
    g_dposlab_sc1_5_1_capture.mode_588 = state[0x588U];
    g_dposlab_sc1_5_1_capture.scale_01c = *scale;

    if (state[0x1B1U] != 8U ||
        *source != 8U ||
        state[0x588U] != 0x17U ||
        *scale != 0x005FU ||
        *key_lo != 0x4C1A6700U ||
        *key_hi != 0x00000037U) {
        profile_sc1_5_1_error = 1U;
        __asm__ volatile ("dmb sy" ::: "memory");
        return;
    }

    profile_sc1_5_1_error = 0U;

    /* Common tail of the selected 0x4003F4DC branch. */
    state[0x6E4U] = 64U - 4U;
    state[0x6E5U] = 64U - 5U;
    state[0x6E6U] = 0x3EU;
    state[0x6E7U] = 1U;
    *(volatile uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x6E8U) = 0U;

    g_dposlab_sc1_5_1_capture.out_6e4_6e7 =
        *(volatile const uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x6E4U);
    g_dposlab_sc1_5_1_capture.out_6e8 =
        *(volatile const uint32_t *)(uintptr_t)(PROFILE_STATE_BASE + 0x6E8U);

    __asm__ volatile ("dmb sy" ::: "memory");
}

static void timing_profile_init(void)
{
    volatile uint8_t *state8 =
        (volatile uint8_t *)(uintptr_t)0x40544E68U;
    volatile uint32_t *state32 =
        (volatile uint32_t *)(uintptr_t)0x40544E68U;

    

    if (state8[0x1B0U] != 1U ||
        state8[0x1B1U] != 8U ||
        state8[0x588U] != 0x17U ||
        state32[0x524U / 4U] != 8U ||
        state32[0x554U / 4U] != 0x00002710U) {
        profile_sc1_3_error = 1U;
        __asm__ volatile ("dmb sy" ::: "memory");
        return;
    }

    profile_sc1_3_error = 0U;

    state32[0x538U / 4U] = 0x4C1A6700U;
    state32[0x53CU / 4U] = 0x00000037U;

    __asm__ volatile ("dmb sy" ::: "memory");
}
int profile_init(
    struct profile_init_result *result,
    profile_progress_fn progress)
{
    struct profile_init_result local;
    struct profile_init_result *out = result ? result : &local;

    out->completed_steps = 0U;
    out->failed_step = 0U;
    out->success = 0;
    out->error = 0;
    profile_sc1_3_error = 0U;
    profile_sc1_5_1_error = 0U;
    profile_sc1_5_3_error = 0U;
    active_progress = progress;
    active_completed_steps = 0U;

    profile_seed_confirmed_inputs();

    if (progress) progress(out->completed_steps, PROFILE_STEP_TIMEBASE);
    timebase_table_init();
    out->completed_steps |= PROFILE_STEP_TIMEBASE;
    active_completed_steps = out->completed_steps;
    if (progress) progress(out->completed_steps, PROFILE_STEP_MODE);

    profile_mode_init();
    out->completed_steps |= PROFILE_STEP_MODE;
    active_completed_steps = out->completed_steps;
    if (progress) progress(out->completed_steps, PROFILE_STEP_TIMING);

    timing_profile_init();
    if (profile_sc1_3_error) {
        out->failed_step = PROFILE_STEP_TIMING;
        out->error = -1;
        if (progress) progress(out->completed_steps, out->failed_step);
        return 0;
    }
    out->completed_steps |= PROFILE_STEP_TIMING;
    active_completed_steps = out->completed_steps;
    if (progress) progress(out->completed_steps, PROFILE_STEP_LAYOUT);

    acquisition_layout_init();
    if (profile_sc1_5_1_error) {
        out->failed_step = PROFILE_STEP_LAYOUT;
        out->error = -2;
        if (progress) progress(out->completed_steps, out->failed_step);
        return 0;
    }
    out->completed_steps |= PROFILE_STEP_LAYOUT;
    active_completed_steps = out->completed_steps;
    if (progress) progress(out->completed_steps, PROFILE_STEP_FPGA_APPLY);

    fpga_profile_apply();
    out->completed_steps |= PROFILE_STEP_FPGA_APPLY;
    active_completed_steps = out->completed_steps;
    if (progress) progress(out->completed_steps, PROFILE_STEP_CHANNEL_ORDER);

    channel_order_init();
    if (profile_sc1_5_3_error) {
        out->failed_step = PROFILE_STEP_CHANNEL_ORDER;
        out->error = -3;
        if (progress) progress(out->completed_steps, out->failed_step);
        return 0;
    }
    out->completed_steps |= PROFILE_STEP_CHANNEL_ORDER;
    active_completed_steps = out->completed_steps;
    if (progress) progress(out->completed_steps, PROFILE_STEP_CHANNEL_CAL);

    channel_calibration_apply();
    out->completed_steps |= PROFILE_STEP_CHANNEL_CAL;
    active_completed_steps = out->completed_steps;
    out->success = 1;
    if (progress) progress(out->completed_steps, 0U);
    active_progress = 0;
    return 1;
}
