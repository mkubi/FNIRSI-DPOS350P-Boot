#include <stdint.h>
#include "smec_matrix.h"

#define SMEC_STATE_BASE ((volatile uint8_t *)0x42E62C2CU)
#define SMEC_STATE_SIZE 0x8FCU

#define STOCK_S2_LOCAL_A ((void (*)(void))0x40049F44U)
#define STOCK_S3_PREAMBLE ((void (*)(void))0x4004A13CU)
#define STOCK_S2_LOCAL_B ((void (*)(uint32_t))0x40049FB8U)
#define STOCK_S4_JOBS ((void (*)(void))0x40049E30U)

volatile struct dposlab_smec_matrix_result dposlab_smec_matrix_result;

const char *dposlab_smec_impl_name(unsigned impl)
{
    switch (impl) {
    case SMEC_IMPL_STOCK: return "STOCK";
    case SMEC_IMPL_CUSTOM: return "CUSTOM";
    case SMEC_IMPL_SKIP: return "SKIP";
    case SMEC_IMPL_EXACT: return "EXACT";
    default: return "INVALID";
    }
}

static void __attribute__((unused)) clear_state_exact(void)
{
    uint32_t i;
    for (i = 0U; i < SMEC_STATE_SIZE; ++i)
        SMEC_STATE_BASE[i] = 0U;
}

static void phase_s1(void)
{
#if SMEC_S1 == SMEC_IMPL_STOCK || SMEC_S1 == SMEC_IMPL_EXACT || SMEC_S1 == SMEC_IMPL_CUSTOM
    clear_state_exact();
#elif SMEC_S1 == SMEC_IMPL_SKIP
#else
#error "SMEC_S1 supports STOCK/EXACT/CUSTOM/SKIP"
#endif
}

static void phase_s2(void)
{
#if SMEC_S2 == SMEC_IMPL_STOCK || SMEC_S2 == SMEC_IMPL_EXACT
    STOCK_S2_LOCAL_A();
    STOCK_S2_LOCAL_B(2000U);
#elif SMEC_S2 == SMEC_IMPL_SKIP
#elif SMEC_S2 == SMEC_IMPL_CUSTOM
    /* No independent custom local-cache initializer exists yet. */
#else
#error "invalid SMEC_S2"
#endif
}

static void __attribute__((unused)) custom_preamble(void)
{
    fpga_test_custom_select_command(0x67U);
    fpga_test_custom_write_payload(0x08U);
    fpga_test_custom_select_command(0x69U);
    fpga_test_custom_write_payload(0x07U);
    fpga_test_custom_write_payload(0xD0U);
    fpga_bus_safe_idle();
}

static void phase_s3(void)
{
#if SMEC_S3 == SMEC_IMPL_STOCK || SMEC_S3 == SMEC_IMPL_EXACT
    STOCK_S3_PREAMBLE();
#elif SMEC_S3 == SMEC_IMPL_CUSTOM
    custom_preamble();
#elif SMEC_S3 == SMEC_IMPL_SKIP
#else
#error "invalid SMEC_S3"
#endif
}

static int phase_s4(struct fpga_smec_init_result *r)
{
#if SMEC_S4 == SMEC_IMPL_STOCK || SMEC_S4 == SMEC_IMPL_EXACT
    STOCK_S4_JOBS();
    return 1;
#elif SMEC_S4 == SMEC_IMPL_CUSTOM
    /* Current custom jobs implementation includes its own verified preamble. */
    return fpga_smec_init_baseline(r);
#elif SMEC_S4 == SMEC_IMPL_SKIP
    (void)r;
    return 1;
#else
#error "invalid SMEC_S4"
#endif
}

static void summarize_state(uint32_t *checksum, uint32_t *nonzero)
{
    uint32_t i;
    uint32_t sum = 2166136261U;
    uint32_t nz = 0U;
    for (i = 0U; i < SMEC_STATE_SIZE; ++i) {
        uint8_t value = SMEC_STATE_BASE[i];
        sum = (sum ^ value) * 16777619U;
        if (value != 0U)
            ++nz;
    }
    *checksum = sum;
    *nonzero = nz;
}

void dposlab_smec_matrix_init(void)
{
    struct fpga_smec_init_result local;
    volatile struct dposlab_smec_matrix_result *out = &dposlab_smec_matrix_result;
    int ok;

    local.completed_jobs = 0U;
    local.attempts = 0U;
    local.poll64_count = 0U;
    local.poll65_count = 0U;
    local.result66 = 0U;
    local.timing = 0U;
    local.current_job = 0U;
    local.status64 = 0U;
    local.final65 = 0U;
    local.echo68 = 0U;
    local.status67 = 0U;
    local.success = 0;
    local.error = 0;

    phase_s1();
    phase_s2();
    phase_s3();
    ok = phase_s4(&local);

    out->custom = local;
    out->s1 = (uint8_t)SMEC_S1;
    out->s2 = (uint8_t)SMEC_S2;
    out->s3 = (uint8_t)SMEC_S3;
    out->s4 = (uint8_t)SMEC_S4;
    summarize_state((uint32_t *)&out->state_checksum,
                    (uint32_t *)&out->state_nonzero);
#if SMEC_S4 == SMEC_IMPL_CUSTOM
    out->success = ok && local.success && local.completed_jobs == 7U;
#else
    out->success = ok;
#endif
}
