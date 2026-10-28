#include <stdint.h>

#include "sysconfig_group_matrix.h"
#include "../../drivers/fpga_protocol.h"

typedef void (*stock_void_fn_t)(void);

volatile dposlab_sc1_3_capture_t g_dposlab_sc1_3_capture
    __attribute__((section(".data"), used));
volatile uint32_t g_dposlab_sc1_3_profile_error
    __attribute__((section(".data"), used));
volatile uint32_t g_dposlab_sc1_3_t1_ran
    __attribute__((section(".data"), used));

volatile dposlab_sc1_5_3_capture_t
    g_dposlab_sc1_5_3_capture
    __attribute__((section(".data"), used));
volatile dposlab_sc1_5_1_capture_t
    g_dposlab_sc1_5_1_capture
    __attribute__((section(".data"), used));
volatile uint32_t g_dposlab_sc1_5_1_profile_error
    __attribute__((section(".data"), used));
volatile uint32_t g_dposlab_sc1_5_3_profile_error
    __attribute__((section(".data"), used));

#if SYSCFG_SC1_5 == SYSCFG_GROUP_STOCK
static void capture_sc1_5_3_inputs(void)
{
    volatile const uint8_t *state8 =
        (volatile const uint8_t *)(uintptr_t)0x40544E68U;
    volatile const uint32_t *key =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x538U);
    volatile const uint32_t *rate =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x53CU);

    g_dposlab_sc1_5_3_capture.magic = DPOSLAB_SC1_5_3_CAPTURE_MAGIC;
    g_dposlab_sc1_5_3_capture.mode_1b0 = state8[0x1B0U];
    g_dposlab_sc1_5_3_capture.key_538 = *key;
    g_dposlab_sc1_5_3_capture.rate_53c = *rate;

    __asm__ volatile ("dmb sy" ::: "memory");
}
#endif

static void __attribute__((unused)) call_stock(uint32_t address)
{
    ((stock_void_fn_t)(uintptr_t)address)();
}

#if SYSCFG_SC1_3 == SYSCFG_GROUP_STOCK
static void capture_and_call_stock_sc1_3(void)
{
    volatile const uint8_t *state8 =
        (volatile const uint8_t *)(uintptr_t)0x40544E68U;
    volatile const uint16_t *param_550 =
        (volatile const uint16_t *)(uintptr_t)(0x40544E68U + 0x550U);
    volatile const uint32_t *source_524 =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x524U);
    volatile const uint32_t *key_lo_538 =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x538U);
    volatile const uint32_t *key_hi_53c =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x53CU);
    volatile const uint32_t *param_558 =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x558U);

    g_dposlab_sc1_3_capture.magic = DPOSLAB_SC1_3_CAPTURE_MAGIC;
    g_dposlab_sc1_3_capture.pre_family_1b0 = state8[0x1B0U];
    g_dposlab_sc1_3_capture.pre_divisor_1b1 = state8[0x1B1U];
    g_dposlab_sc1_3_capture.pre_index_588 = state8[0x588U];
    g_dposlab_sc1_3_capture.pre_source_524 = *source_524;

    call_stock(0x4003A984U);

    g_dposlab_sc1_3_capture.post_key_lo_538 = *key_lo_538;
    g_dposlab_sc1_3_capture.post_key_hi_53c = *key_hi_53c;
    g_dposlab_sc1_3_capture.post_param_550 = *param_550;
    g_dposlab_sc1_3_capture.post_param_558 = *param_558;

    g_dposlab_sc1_3_capture.post_530_lo =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x530U);
    g_dposlab_sc1_3_capture.post_534_hi =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x534U);
    g_dposlab_sc1_3_capture.post_554 =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x554U);
    g_dposlab_sc1_3_capture.post_55c =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x55CU);
    g_dposlab_sc1_3_capture.post_560 =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x560U);
    g_dposlab_sc1_3_capture.post_568_lo =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x568U);
    g_dposlab_sc1_3_capture.post_56c_hi =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x56CU);
    g_dposlab_sc1_3_capture.post_570_lo =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x570U);
    g_dposlab_sc1_3_capture.post_574_hi =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x574U);
    g_dposlab_sc1_3_capture.post_128_lo =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x128U);
    g_dposlab_sc1_3_capture.post_12c_hi =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x12CU);
    g_dposlab_sc1_3_capture.post_068_lo =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x068U);
    g_dposlab_sc1_3_capture.post_06c_hi =
        *(volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x06CU);

    __asm__ volatile ("dmb sy" ::: "memory");
}
#endif

#if SYSCFG_SC1_5 == SYSCFG_GROUP_STOCK
static void capture_and_call_stock_sc1_5_1(void)
{
    volatile const uint8_t *state8 =
        (volatile const uint8_t *)(uintptr_t)0x40544E68U;
    volatile const uint16_t *scale =
        (volatile const uint16_t *)(uintptr_t)(0x40544E68U + 0x1CU);
    volatile const uint32_t *source =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x524U);
    volatile const uint32_t *out_word =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x6E4U);
    volatile const uint32_t *out_6e8 =
        (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x6E8U);

    g_dposlab_sc1_5_1_capture.magic = DPOSLAB_SC1_5_1_CAPTURE_MAGIC;
    g_dposlab_sc1_5_1_capture.divisor_1b1 = state8[0x1B1U];
    g_dposlab_sc1_5_1_capture.source_524 = *source;
    g_dposlab_sc1_5_1_capture.mode_588 = state8[0x588U];
    g_dposlab_sc1_5_1_capture.scale_01c = *scale;

    call_stock(0x4003F2F4U);

    g_dposlab_sc1_5_1_capture.out_6e4_6e7 = *out_word;
    g_dposlab_sc1_5_1_capture.out_6e8 = *out_6e8;
    __asm__ volatile ("dmb sy" ::: "memory");
}
#endif

#define STOCK_APP_STATE_BASE 0x40544E68U

#if SYSCFG_SC1_2 == SYSCFG_GROUP_CUSTOM
/* Exact stock-like replacement of SC1.2 @ 0x4003A908. */
static void custom_sc1_2(void)
{
    volatile uint8_t *state8 =
        (volatile uint8_t *)(uintptr_t)STOCK_APP_STATE_BASE;
    volatile uint32_t *state32 =
        (volatile uint32_t *)(uintptr_t)STOCK_APP_STATE_BASE;
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
#endif

#if SYSCFG_SC1_5 == SYSCFG_GROUP_CUSTOM
#define STOCK_PARAM_WRITE      ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(uintptr_t)0x40049EC4U)
#define STOCK_PARAM_COMMIT     ((void (*)(uint32_t))(uintptr_t)0x40049F1CU)
#define STOCK_POST_APPLY       ((void (*)(uint32_t))(uintptr_t)0x40042CF0U)

static void custom_sc1_5_2_write_byte(volatile uint8_t *state,
                                     uint32_t source_offset,
                                     uint32_t shadow_offset,
                                     uint32_t parameter,
                                     uint32_t commit,
                                     uint32_t selector,
                                     uint32_t *changed)
{
    uint32_t byte_value = state[source_offset];
    uint32_t shadow_window;

    if (state[shadow_offset] == (uint8_t)byte_value)
        return;

    state[shadow_offset] = (uint8_t)byte_value;

    /*
     * Stock call ABI for 0x40049EC4:
     *
     *   r0 = parameter
     *   r1 = 1
     *   r2 = 32-bit shadow window
     *   r3 = source byte
     *
     * Because r1 is below 4, 0x40049EC4 selects r3 as the
     * effective value. Preserve r2 as well for exact ABI parity.
     */
    shadow_window =
        ((uint32_t)state[shadow_offset + 0U]      ) |
        ((uint32_t)state[shadow_offset + 1U] <<  8) |
        ((uint32_t)state[shadow_offset + 2U] << 16) |
        ((uint32_t)state[shadow_offset + 3U] << 24);

    STOCK_PARAM_WRITE(
        parameter,
        1U,
        shadow_window,
        byte_value);
    STOCK_PARAM_COMMIT(commit);
    fpga_protocol_write_u8((uint8_t)selector, (uint8_t)byte_value);
    *changed = 1U;
}

/* Exact stock-like replacement of SC1.5.2 @ 0x4003F160. */
static void custom_sc1_5_2(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)STOCK_APP_STATE_BASE;
    volatile uint32_t *source_word =
        (volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x6E8U);
    volatile uint32_t *shadow_word =
        (volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0xBB4U);
    uint32_t changed = 0U;
    uint32_t value;

    custom_sc1_5_2_write_byte(state, 0x6E4U, 0xBB0U,
                              0x21U, 0xC1U, 0x04U, &changed);
    custom_sc1_5_2_write_byte(state, 0x6E5U, 0xBB1U,
                              0x22U, 0xC2U, 0x05U, &changed);
    custom_sc1_5_2_write_byte(state, 0x6E6U, 0xBB2U,
                              0x23U, 0xC3U, 0x06U, &changed);

    value = state[0x6E7U];
    if (state[0xBB3U] != (uint8_t)value) {
        state[0xBB3U] = (uint8_t)value;
        fpga_protocol_write_u8(0x02U, (uint8_t)value);
        changed = 1U;
    }

    value = *source_word;
    if (*shadow_word != value) {
        *shadow_word = value;
        fpga_protocol_write_u32_be(0x0DU, value);
        changed = 1U;
    }

    if (changed != 0U) {
        fpga_protocol_write_u8(0x07U, 1U);
        fpga_protocol_write_u8(0x07U, 0U);
        STOCK_POST_APPLY(2U);
    }
}
#endif

#if SYSCFG_SC1_5 == SYSCFG_GROUP_CUSTOM
#ifndef STOCK_PARAM_WRITE
#define STOCK_PARAM_WRITE      ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(uintptr_t)0x40049EC4U)
#endif
#ifndef STOCK_PARAM_COMMIT
#define STOCK_PARAM_COMMIT     ((void (*)(uint32_t))(uintptr_t)0x40049F1CU)
#endif

static uint32_t custom_load_u32_le(const volatile uint8_t *p)
{
    return
        ((uint32_t)p[0]      ) |
        ((uint32_t)p[1] <<  8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

/* Exact stock-like replacement of SC1.5.4 @ 0x40063CD0. */
static void custom_sc1_5_4(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)STOCK_APP_STATE_BASE;
    uint32_t i;

    for (i = 0U; i < 8U; ++i) {
        uint32_t source_offset = 0x674U + i;
        uint32_t shadow_offset = 0xBB8U + i;
        uint32_t byte_value = state[source_offset];
        uint32_t shadow_window;

        if (state[shadow_offset] == (uint8_t)byte_value)
            continue;

        state[shadow_offset] = (uint8_t)byte_value;

        /*
         * Preserve the stock call ABI of 0x40049EC4:
         *   r0 = 0x19 + i
         *   r1 = 1
         *   r2 = little-endian u32 window beginning at shadow byte
         *   r3 = source byte
         */
        shadow_window = custom_load_u32_le(&state[shadow_offset]);
        STOCK_PARAM_WRITE(0x19U + i, 1U, shadow_window, byte_value);
        STOCK_PARAM_COMMIT(0xB9U + i);

        fpga_protocol_write_u8(
            (uint8_t)(0xA6U + i),
            (uint8_t)byte_value);
    }

    /* Stock SC1.5.4 always emits the final selector 0x11 pulse. */
    fpga_protocol_write_u8(0x11U, 0U);
    fpga_protocol_write_u8(0x11U, 1U);
}
#endif

#if SYSCFG_SC1_5 == SYSCFG_GROUP_CUSTOM
/*
 * Minimal stock-like replacement of SC1.5.3 @ 0x40063F04.
 *
 * Supported HW-confirmed profile:
 *   APP_STATE +0x1B0 = 0x01
 *   APP_STATE +0x538 = 0x4C1A6700
 *   APP_STATE +0x53C = 0x00000037
 *
 * No stock fallback is used. Unknown profiles fail closed and leave the
 * output state untouched.
 */
static void custom_sc1_5_3(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)STOCK_APP_STATE_BASE;
    volatile const uint32_t *key =
        (volatile const uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x538U);
    volatile const uint32_t *rate =
        (volatile const uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x53CU);
    uint32_t i;

    g_dposlab_sc1_5_3_capture.magic = DPOSLAB_SC1_5_3_CAPTURE_MAGIC;
    g_dposlab_sc1_5_3_capture.mode_1b0 = state[0x1B0U];
    g_dposlab_sc1_5_3_capture.key_538 = *key;
    g_dposlab_sc1_5_3_capture.rate_53c = *rate;

    if (state[0x1B0U] != 1U ||
        *key != 0x4C1A6700U ||
        *rate != 0x00000037U) {
        g_dposlab_sc1_5_3_profile_error = 1U;
        __asm__ volatile ("dmb sy" ::: "memory");
        return;
    }

    g_dposlab_sc1_5_3_profile_error = 0U;

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

    *(volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x6D4U) = 0U;
    *(volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x6D8U) = 0U;

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
#endif

#if SYSCFG_SC1_5 == SYSCFG_GROUP_CUSTOM
/*
 * Minimal stock-like replacement of SC1.5.1 @ 0x4003F2F4.
 *
 * HW-confirmed input profile:
 *   +0x1B1 = 8
 *   +0x524 = 8
 *   +0x588 = 0x17
 *   +0x01C = 95
 *   +0x538:+0x53C = 0x000000374C1A6700
 *
 * Stock math produces lookup key 0x0000001D1A94A200. The matching
 * table entry at 0x4003F454 has displacement 0x7C and dispatches to
 * 0x4003F4DC. That branch selects r4=4, r5=5, r6=1, r7=0 and
 * stack parameter 0x3E. The common tail stores the values below.
 *
 * No stock fallback is used. Unknown profiles fail closed.
 */
static void custom_sc1_5_1(void)
{
    volatile uint8_t *state =
        (volatile uint8_t *)(uintptr_t)STOCK_APP_STATE_BASE;
    volatile const uint16_t *scale =
        (volatile const uint16_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x1CU);
    volatile const uint32_t *source =
        (volatile const uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x524U);
    volatile const uint32_t *key_lo =
        (volatile const uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x538U);
    volatile const uint32_t *key_hi =
        (volatile const uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x53CU);

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
        g_dposlab_sc1_5_1_profile_error = 1U;
        __asm__ volatile ("dmb sy" ::: "memory");
        return;
    }

    g_dposlab_sc1_5_1_profile_error = 0U;

    /* Common tail of the selected 0x4003F4DC branch. */
    state[0x6E4U] = 64U - 4U;
    state[0x6E5U] = 64U - 5U;
    state[0x6E6U] = 0x3EU;
    state[0x6E7U] = 1U;
    *(volatile uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x6E8U) = 0U;

    g_dposlab_sc1_5_1_capture.out_6e4_6e7 =
        *(volatile const uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x6E4U);
    g_dposlab_sc1_5_1_capture.out_6e8 =
        *(volatile const uint32_t *)(uintptr_t)(STOCK_APP_STATE_BASE + 0x6E8U);

    __asm__ volatile ("dmb sy" ::: "memory");
}
#endif

#if SYSCFG_SC1_3 == SYSCFG_GROUP_CUSTOM
/*
 * SC1.3 profile smoke test T6.
 *
 * Reproduces only the HW-confirmed primary dispatch branch 0x4003ABC4:
 *   +0x538:+0x53C = 0x000000374C1A6700
 *   +0x550        = 4
 *   +0x558        = 50
 *   +0x55C        = +0x554 (stock shadow copy)
 *   +0x530:+0x534 = normalized timing
 *   +0x560        = integer multiplier 1
 *   +0x568        = signed lower bound -475
 *   +0x56C        = signed upper bound +475
 *   +0x570:+0x574 = zero position/offset
 *   +0x128:+0x12C = final timing limit 350000000
 *   +0x068:+0x06C = final timing limit 350000000
 *   stock 0x4004C544 = RAM-only horizontal/profile table materializer
 *   +0x640 (u16) = 0x17FA, stock stream geometry input A
 *   +0x6F2 (u16) = 0x0003, stock stream geometry input B
 *
 * For the HW-confirmed golden profile, stock 0x400405E0 therefore computes:
 *   CSI geometry = A + 2*B       = 0x1800
 *   FPGA quantity = 20 + 2(A+B) = 0x300E
 *
 * T5 intentionally retains one isolated stock helper call. Static analysis
 * confirms that 0x4004C544 has no nested calls and performs RAM reads/writes
 * only, materializing the 0x50-byte block at 0x42E53A0C.
 *
 * For the confirmed profile +0x554 == +0x55C == 10000, therefore the
 * normalized timing equals +0x538:+0x53C exactly. No 64-bit multiply is
 * needed in this profile-specific smoke test.
 *
 * No stock call and no fallback. The complete profile is validated before
 * the first APP_STATE write. Unknown profiles fail closed.
 */


static void custom_sc1_3(void)
{
    volatile uint8_t *state8 =
        (volatile uint8_t *)(uintptr_t)0x40544E68U;
    volatile uint32_t *state32 =
        (volatile uint32_t *)(uintptr_t)0x40544E68U;

    g_dposlab_sc1_3_t1_ran = 1U;

    if (state8[0x1B0U] != 1U ||
        state8[0x1B1U] != 8U ||
        state8[0x588U] != 0x17U ||
        state32[0x524U / 4U] != 8U ||
        state32[0x554U / 4U] != 0x00002710U) {
        g_dposlab_sc1_3_profile_error = 1U;
        __asm__ volatile ("dmb sy" ::: "memory");
        return;
    }

    g_dposlab_sc1_3_profile_error = 0U;

    state32[0x538U / 4U] = 0x4C1A6700U;
    state32[0x53CU / 4U] = 0x00000037U;

    __asm__ volatile ("dmb sy" ::: "memory");
}
#endif

void __attribute__((noinline)) dposlab_sysconfig_group_dispatch(void)
{
#if SYSCFG_G1 == SYSCFG_GROUP_STOCK
    /*
     * Stock SC1 @ 0x4003A654 decomposed without diagnostic side effects.
     * Preserve the original helper order exactly.
     */
#if SYSCFG_SC1_1 == SYSCFG_GROUP_STOCK
    call_stock(0x4003BE78U);
#endif
#if SYSCFG_SC1_2 == SYSCFG_GROUP_STOCK
    call_stock(0x4003A908U);
#elif SYSCFG_SC1_2 == SYSCFG_GROUP_CUSTOM
    custom_sc1_2();
#endif
#if SYSCFG_SC1_3 == SYSCFG_GROUP_STOCK
    capture_and_call_stock_sc1_3();
#elif SYSCFG_SC1_3 == SYSCFG_GROUP_CUSTOM
    custom_sc1_3();
#endif
#if SYSCFG_SC1_4 == SYSCFG_GROUP_STOCK
    /* Stock SC1 loads r0/r1 from APP_STATE +0x30 before this call. */
    {
        typedef void (*stock_sc1_4_fn_t)(uint32_t, uint32_t);
        volatile const uint32_t *state =
            (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x30U);
        ((stock_sc1_4_fn_t)(uintptr_t)0x4003F948U)(state[0], state[1]);
    }
#endif
#if SYSCFG_SC1_5 == SYSCFG_GROUP_STOCK
    call_stock(0x4003F894U);
#elif SYSCFG_SC1_5 == SYSCFG_GROUP_CUSTOM
    /* HW-confirmed minimal SC1.5 order. SC1.5.5 is not required. */
    custom_sc1_5_1();
    custom_sc1_5_2();
    custom_sc1_5_3();
    custom_sc1_5_4();
#endif
#endif
#if SYSCFG_G2 == SYSCFG_GROUP_STOCK
    call_stock(0x4004A510U);
#endif
#if SYSCFG_G3 == SYSCFG_GROUP_STOCK
    call_stock(0x40063328U);
#endif
}
