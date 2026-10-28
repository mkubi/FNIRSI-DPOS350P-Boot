#ifndef DPOSLAB_SYSCONFIG_GROUP_MATRIX_H
#define DPOSLAB_SYSCONFIG_GROUP_MATRIX_H

#define SYSCFG_GROUP_STOCK  0
#define SYSCFG_GROUP_CUSTOM 1
#define SYSCFG_GROUP_SKIP   2

#ifndef SYSCFG_SC1_1
#define SYSCFG_SC1_1 SYSCFG_GROUP_STOCK
#endif
#ifndef SYSCFG_SC1_2
#define SYSCFG_SC1_2 SYSCFG_GROUP_STOCK
#endif
#ifndef SYSCFG_SC1_3
#define SYSCFG_SC1_3 SYSCFG_GROUP_STOCK
#endif
#ifndef SYSCFG_SC1_4
#define SYSCFG_SC1_4 SYSCFG_GROUP_STOCK
#endif
#ifndef SYSCFG_SC1_5
#define SYSCFG_SC1_5 SYSCFG_GROUP_STOCK
#endif


#ifndef SYSCFG_G1
#define SYSCFG_G1 SYSCFG_GROUP_STOCK
#endif
#ifndef SYSCFG_G2
#define SYSCFG_G2 SYSCFG_GROUP_STOCK
#endif
#ifndef SYSCFG_G3
#define SYSCFG_G3 SYSCFG_GROUP_STOCK
#endif

#if SYSCFG_G1 != SYSCFG_GROUP_STOCK && SYSCFG_G1 != SYSCFG_GROUP_SKIP
#error "SYSCFG_G1 must be STOCK (0) or SKIP (2)"
#endif
#if SYSCFG_G2 != SYSCFG_GROUP_STOCK && SYSCFG_G2 != SYSCFG_GROUP_SKIP
#error "SYSCFG_G2 must be STOCK (0) or SKIP (2)"
#endif
#if SYSCFG_G3 != SYSCFG_GROUP_STOCK && SYSCFG_G3 != SYSCFG_GROUP_SKIP
#error "SYSCFG_G3 must be STOCK (0) or SKIP (2)"
#endif


#if SYSCFG_SC1_1 != SYSCFG_GROUP_STOCK && SYSCFG_SC1_1 != SYSCFG_GROUP_SKIP
#error "SYSCFG_SC1_1 must be STOCK (0) or SKIP (2)"
#endif
#if SYSCFG_SC1_2 != SYSCFG_GROUP_STOCK && \
    SYSCFG_SC1_2 != SYSCFG_GROUP_CUSTOM && \
    SYSCFG_SC1_2 != SYSCFG_GROUP_SKIP
#error "SYSCFG_SC1_2 must be STOCK (0), CUSTOM (1), or SKIP (2)"
#endif
#if SYSCFG_SC1_3 != SYSCFG_GROUP_STOCK && \
    SYSCFG_SC1_3 != SYSCFG_GROUP_CUSTOM && \
    SYSCFG_SC1_3 != SYSCFG_GROUP_SKIP
#error "SYSCFG_SC1_3 must be STOCK (0), CUSTOM (1), or SKIP (2)"
#endif
#if SYSCFG_SC1_4 != SYSCFG_GROUP_STOCK && SYSCFG_SC1_4 != SYSCFG_GROUP_SKIP
#error "SYSCFG_SC1_4 must be STOCK (0) or SKIP (2)"
#endif
#if SYSCFG_SC1_5 != SYSCFG_GROUP_STOCK && \
    SYSCFG_SC1_5 != SYSCFG_GROUP_CUSTOM && \
    SYSCFG_SC1_5 != SYSCFG_GROUP_SKIP
#error "SYSCFG_SC1_5 must be STOCK (0), CUSTOM (1), or SKIP (2)"
#endif



typedef struct {
    uint32_t magic;
    uint32_t mode_1b0;
    uint32_t key_538;
    uint32_t rate_53c;
} dposlab_sc1_5_3_capture_t;

typedef struct {
    uint32_t magic;
    uint32_t divisor_1b1;
    uint32_t source_524;
    uint32_t mode_588;
    uint32_t scale_01c;
    uint32_t out_6e4_6e7;
    uint32_t out_6e8;
} dposlab_sc1_5_1_capture_t;

#define DPOSLAB_SC1_5_3_CAPTURE_MAGIC 0x53313533U /* S153 */
#define DPOSLAB_SC1_5_1_CAPTURE_MAGIC 0x53313531U /* S151 */

extern volatile dposlab_sc1_5_3_capture_t
    g_dposlab_sc1_5_3_capture;
extern volatile dposlab_sc1_5_1_capture_t
    g_dposlab_sc1_5_1_capture;
extern volatile uint32_t g_dposlab_sc1_5_1_profile_error;
extern volatile uint32_t g_dposlab_sc1_5_3_profile_error;

typedef struct {
    uint32_t magic;

    /* Captured immediately before stock SC1.3 @ 0x4003A984. */
    uint32_t pre_family_1b0;
    uint32_t pre_divisor_1b1;
    uint32_t pre_index_588;
    uint32_t pre_source_524;

    /* Captured immediately after stock SC1.3 returns. */
    uint32_t post_key_lo_538;
    uint32_t post_key_hi_53c;
    uint32_t post_param_550;
    uint32_t post_param_558;

    /* Derived state captured after stock SC1.3 returns. */
    uint32_t post_530_lo;
    uint32_t post_534_hi;
    uint32_t post_554;
    uint32_t post_55c;
    uint32_t post_560;
    uint32_t post_568_lo;
    uint32_t post_56c_hi;
    uint32_t post_570_lo;
    uint32_t post_574_hi;
    uint32_t post_128_lo;
    uint32_t post_12c_hi;
    uint32_t post_068_lo;
    uint32_t post_06c_hi;
} dposlab_sc1_3_capture_t;

#define DPOSLAB_SC1_3_CAPTURE_MAGIC 0x53433133U /* "SC13" */

extern volatile dposlab_sc1_3_capture_t g_dposlab_sc1_3_capture;
extern volatile uint32_t g_dposlab_sc1_3_profile_error;
extern volatile uint32_t g_dposlab_sc1_3_t1_ran;

void dposlab_sysconfig_group_dispatch(void);

#endif
