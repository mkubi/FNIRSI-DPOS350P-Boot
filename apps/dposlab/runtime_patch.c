#include <stdint.h>
#include "runtime_patch.h"
#include "../../arch/r11/cache.h"

#define STOCK_FPGA_CALL_SITE         0x40034738U
#define STOCK_FPGA_CALL_ORIGINAL     0xEB005292U
#define STOCK_SMEC_MATRIX_SITE       0x40034750U
#define STOCK_SMEC_MATRIX_ORIGINAL   0xEB005681U
#define DPOSLAB_SMEC_MATRIX_VENEER   0x40055AECU
#define DPOSLAB_CUSTOM_FPGA_VENEER   0x40055AE4U
#define DPOSLAB_FPGA_VENEER          0x40055B4CU

#define STOCK_FPGA_PHASE_A_SITE      0x400491A8U
#define STOCK_FPGA_PHASE_A_ORIGINAL  0xEBFFFFB9U
#define DPOSLAB_FPGA_PHASE_A_VENEER  0x40055ADCU

#define STOCK_FPGA_EARLY_SITE        0x40049194U
#define STOCK_FPGA_EARLY_ORIGINAL    0xEB0073CBU
#define DPOSLAB_FPGA_EARLY_VENEER    0x40055AE4U

#define STOCK_CSI_CLOCK_RESET_SITE      0x400660D4U
#define STOCK_CSI_CLOCK_RESET_ORIGINAL  0xEBFFFF9BU
#define DPOSLAB_CSI_CLOCK_RESET_VENEER  0x40055AECU

#define STOCK_CSI_BASE_BIND_SITE         0x400660E4U
#define STOCK_CSI_BASE_BIND_ORIGINAL     0xEBFFFDF4U
#define DPOSLAB_CSI_BASE_BIND_VENEER     0x40055AF4U

#define STOCK_CSI_ENABLE_A_SITE           0x400660ECU
#define STOCK_CSI_ENABLE_A_ORIGINAL       0xEBFFFEDDU
#define DPOSLAB_CSI_ENABLE_A_VENEER       0x40055AFCU

#define STOCK_CSI_ENABLE_B_SITE           0x400660F4U
#define STOCK_CSI_ENABLE_B_ORIGINAL       0xEBFFFEDAU
#define DPOSLAB_CSI_ENABLE_B_VENEER       0x40055B04U

#define STOCK_CSI_CONFIG_8000_SITE         0x400660FCU
#define STOCK_CSI_CONFIG_8000_ORIGINAL     0xEBFFFFA9U
#define DPOSLAB_CSI_CONFIG_8000_VENEER     0x40055B0CU

#define STOCK_CSI_PINMUX_SITE              0x40066060U
#define STOCK_CSI_PINMUX_ORIGINAL          0xEBFFFFA4U
#define DPOSLAB_CSI_PINMUX_VENEER     0x40055B14U

#define STOCK_SMEC_ECHO68_SITE          0x4004A0F0U
#define STOCK_SMEC_ECHO68_ORIGINAL      0xEBFFFFDBU
#define DPOSLAB_SMEC_ECHO68_VENEER      0x40055B1CU

#define STOCK_SMEC_VALID67_SITE         0x4004A0F8U
#define STOCK_SMEC_VALID67_ORIGINAL     0xEBFFFFC8U
#define DPOSLAB_SMEC_VALID67_VENEER     0x40055B24U

#define STOCK_SMEC_JOB_START_SITE        0x4004A0E8U
#define STOCK_SMEC_JOB_START_ORIGINAL    0xEBFFFFBBU
#define DPOSLAB_SMEC_JOB_START_VENEER    0x40055B2CU

#define STOCK_SMEC_POLL65_SITE           0x4004A0ECU
#define STOCK_SMEC_POLL65_ORIGINAL       0xEBFFFFD3U
#define DPOSLAB_SMEC_POLL65_VENEER       0x40055B34U

#define STOCK_SCREENSHOT_SITE        0x400350F0U
#define STOCK_SCREENSHOT_ORIGINAL    0xEAFFFC63U
#define DPOSLAB_SCREENSHOT_VENEER    0x40055AD4U

#define ARM_LDR_PC_PC_MINUS_4        0xE51FF004U

extern void dposlab_fpga_start_hook(void);
extern void dposlab_fpga_phase_a_hook(void);
extern void dposlab_fpga_early_hook(void);
extern void dposlab_csi_clock_reset_hook(void);
extern void dposlab_csi_base_bind_hook(void);
extern void dposlab_csi_enable_a_hook(void);
extern void dposlab_csi_enable_b_hook(void);
extern void dposlab_csi_config_8000_hook(void);
extern void dposlab_csi_pinmux_hook(void);
extern void dposlab_smec_echo68_hook(void);
extern void dposlab_smec_valid67_hook(void);
extern void dposlab_smec_job_start_hook(void);
extern void dposlab_smec_poll65_hook(void);
extern void dposlab_screenshot_hook(void);

static volatile uint32_t *word_at(uint32_t address)
{
    return (volatile uint32_t *)(uintptr_t)address;
}

static int encode_arm_branch(uint32_t site,
                             uint32_t target,
                             uint32_t link,
                             uint32_t *opcode)
{
    int32_t displacement;
    int32_t word_offset;
    uint32_t base_opcode;

    displacement = (int32_t)(target - (site + 8U));

    if ((displacement & 3) != 0)
        return 0;

    /*
     * ARM B/BL immediate range:
     *
     *     -0x02000000 .. +0x01FFFFFC
     */
    if (displacement < -0x02000000 ||
        displacement > 0x01FFFFFC)
        return 0;

    word_offset = displacement >> 2;
    base_opcode = link != 0U ? 0xEB000000U : 0xEA000000U;

    *opcode =
        base_opcode |
        ((uint32_t)word_offset & 0x00FFFFFFU);

    return 1;
}

static void install_absolute_veneer(uint32_t veneer_address,
                                    uint32_t target_address)
{
    volatile uint32_t *veneer;

    veneer = word_at(veneer_address);

    veneer[0] = ARM_LDR_PC_PC_MINUS_4;
    veneer[1] = target_address;
}

int dposlab_install_fpga_hook(void)
{
    volatile uint32_t *fpga_site;
    volatile uint32_t *early_site;
    volatile uint32_t *csi_clock_reset_site;
    volatile uint32_t *csi_base_bind_site;
    volatile uint32_t *csi_enable_a_site;
    volatile uint32_t *csi_enable_b_site;
    volatile uint32_t *csi_config_8000_site;
    volatile uint32_t *csi_pinmux_site;
    volatile uint32_t *smec_echo68_site;
    volatile uint32_t *smec_valid67_site;
    volatile uint32_t *smec_job_start_site;
    volatile uint32_t *smec_poll65_site;
    volatile uint32_t *phase_a_site;
    volatile uint32_t *screenshot_site;

    volatile uint32_t *fpga_veneer;
    volatile uint32_t *early_veneer;
    volatile uint32_t *csi_clock_reset_veneer;
    volatile uint32_t *csi_base_bind_veneer;
    volatile uint32_t *csi_enable_a_veneer;
    volatile uint32_t *csi_enable_b_veneer;
    volatile uint32_t *csi_config_8000_veneer;
    volatile uint32_t *csi_pinmux_veneer;
    volatile uint32_t *smec_echo68_veneer;
    volatile uint32_t *smec_valid67_veneer;
    volatile uint32_t *smec_job_start_veneer;
    volatile uint32_t *smec_poll65_veneer;
    volatile uint32_t *phase_a_veneer;
    volatile uint32_t *screenshot_veneer;

    uint32_t fpga_hook_address;
    uint32_t early_hook_address;
    uint32_t csi_clock_reset_hook_address;
    uint32_t csi_base_bind_hook_address;
    uint32_t csi_enable_a_hook_address;
    uint32_t csi_enable_b_hook_address;
    uint32_t csi_config_8000_hook_address;
    uint32_t csi_pinmux_hook_address;
    uint32_t smec_echo68_hook_address;
    uint32_t smec_valid67_hook_address;
    uint32_t smec_job_start_hook_address;
    uint32_t smec_poll65_hook_address;
    uint32_t phase_a_hook_address;
    uint32_t screenshot_hook_address;

    uint32_t fpga_branch;
    uint32_t early_branch;
    uint32_t csi_clock_reset_branch;
    uint32_t csi_base_bind_branch;
    uint32_t csi_enable_a_branch;
    uint32_t csi_enable_b_branch;
    uint32_t csi_config_8000_branch;
    uint32_t csi_pinmux_branch;
    uint32_t smec_echo68_branch;
    uint32_t smec_valid67_branch;
    uint32_t smec_job_start_branch;
    uint32_t smec_poll65_branch;
    uint32_t phase_a_branch;
    uint32_t screenshot_branch;

    fpga_site = word_at(STOCK_FPGA_CALL_SITE);
    early_site = word_at(STOCK_FPGA_EARLY_SITE);
    csi_clock_reset_site =
        word_at(STOCK_CSI_CLOCK_RESET_SITE);
    csi_base_bind_site =
        word_at(STOCK_CSI_BASE_BIND_SITE);
    csi_enable_a_site =
        word_at(STOCK_CSI_ENABLE_A_SITE);
    csi_enable_b_site =
        word_at(STOCK_CSI_ENABLE_B_SITE);
    csi_config_8000_site =
        word_at(STOCK_CSI_CONFIG_8000_SITE);
    csi_pinmux_site =
        word_at(STOCK_CSI_PINMUX_SITE);
    smec_echo68_site = word_at(STOCK_SMEC_ECHO68_SITE);
    smec_valid67_site = word_at(STOCK_SMEC_VALID67_SITE);
    smec_job_start_site = word_at(STOCK_SMEC_JOB_START_SITE);
    smec_poll65_site = word_at(STOCK_SMEC_POLL65_SITE);
    phase_a_site = word_at(STOCK_FPGA_PHASE_A_SITE);
    screenshot_site = word_at(STOCK_SCREENSHOT_SITE);

    fpga_veneer = word_at(DPOSLAB_FPGA_VENEER);
    early_veneer = word_at(DPOSLAB_FPGA_EARLY_VENEER);
    csi_clock_reset_veneer =
        word_at(DPOSLAB_CSI_CLOCK_RESET_VENEER);
    csi_base_bind_veneer =
        word_at(DPOSLAB_CSI_BASE_BIND_VENEER);
    csi_enable_a_veneer =
        word_at(DPOSLAB_CSI_ENABLE_A_VENEER);
    csi_enable_b_veneer =
        word_at(DPOSLAB_CSI_ENABLE_B_VENEER);
    csi_config_8000_veneer =
        word_at(DPOSLAB_CSI_CONFIG_8000_VENEER);
    csi_pinmux_veneer =
        word_at(DPOSLAB_CSI_PINMUX_VENEER);
    smec_echo68_veneer = word_at(DPOSLAB_SMEC_ECHO68_VENEER);
    smec_valid67_veneer = word_at(DPOSLAB_SMEC_VALID67_VENEER);
    smec_job_start_veneer = word_at(DPOSLAB_SMEC_JOB_START_VENEER);
    smec_poll65_veneer = word_at(DPOSLAB_SMEC_POLL65_VENEER);
    phase_a_veneer = word_at(DPOSLAB_FPGA_PHASE_A_VENEER);
    screenshot_veneer = word_at(DPOSLAB_SCREENSHOT_VENEER);

    fpga_hook_address =
        (uint32_t)(uintptr_t)dposlab_fpga_start_hook;

    early_hook_address =
        (uint32_t)(uintptr_t)dposlab_fpga_early_hook;

    csi_clock_reset_hook_address =
        (uint32_t)(uintptr_t)dposlab_csi_clock_reset_hook;

    csi_base_bind_hook_address =
        (uint32_t)(uintptr_t)dposlab_csi_base_bind_hook;

    csi_enable_a_hook_address =
        (uint32_t)(uintptr_t)dposlab_csi_enable_a_hook;

    csi_enable_b_hook_address =
        (uint32_t)(uintptr_t)dposlab_csi_enable_b_hook;

    csi_config_8000_hook_address =
        (uint32_t)(uintptr_t)dposlab_csi_config_8000_hook;

    csi_pinmux_hook_address =
        (uint32_t)(uintptr_t)dposlab_csi_pinmux_hook;
    smec_echo68_hook_address =
        (uint32_t)(uintptr_t)dposlab_smec_echo68_hook;
    smec_valid67_hook_address =
        (uint32_t)(uintptr_t)dposlab_smec_valid67_hook;
    smec_job_start_hook_address =
        (uint32_t)(uintptr_t)dposlab_smec_job_start_hook;
    smec_poll65_hook_address =
        (uint32_t)(uintptr_t)dposlab_smec_poll65_hook;

    phase_a_hook_address =
        (uint32_t)(uintptr_t)dposlab_fpga_phase_a_hook;

    screenshot_hook_address =
        (uint32_t)(uintptr_t)dposlab_screenshot_hook;

    /*
     * Validate the complete patch set before modifying EXE2.
     *
     * If any expected instruction differs, refuse the whole patch.
     * This prevents a partial installation against an incompatible
     * EXE2 image or an already modified call site.
     */
    if (*fpga_site != STOCK_FPGA_CALL_ORIGINAL)
        return 0;

    if (*early_site != STOCK_FPGA_EARLY_ORIGINAL)
        return 0;

    if (*csi_clock_reset_site !=
        STOCK_CSI_CLOCK_RESET_ORIGINAL)
        return 0;

    if (*csi_base_bind_site !=
        STOCK_CSI_BASE_BIND_ORIGINAL)
        return 0;

    if (*csi_enable_a_site !=
        STOCK_CSI_ENABLE_A_ORIGINAL)
        return 0;

    if (*csi_enable_b_site !=
        STOCK_CSI_ENABLE_B_ORIGINAL)
        return 0;

    if (*csi_config_8000_site !=
        STOCK_CSI_CONFIG_8000_ORIGINAL)
        return 0;

    if (*csi_pinmux_site !=
        STOCK_CSI_PINMUX_ORIGINAL)
        return 0;

    if (*smec_echo68_site != STOCK_SMEC_ECHO68_ORIGINAL)
        return 0;

    if (*smec_valid67_site != STOCK_SMEC_VALID67_ORIGINAL)
        return 0;
    if (*smec_job_start_site != STOCK_SMEC_JOB_START_ORIGINAL)
        return 0;
    if (*smec_poll65_site != STOCK_SMEC_POLL65_ORIGINAL)
        return 0;

    if (*phase_a_site != STOCK_FPGA_PHASE_A_ORIGINAL)
        return 0;

    if (*screenshot_site != STOCK_SCREENSHOT_ORIGINAL)
        return 0;

    /*
     * Outer FPGA_Start hook:
     *
     *     0x40034738: BL 0x40049188
     *              -> BL 0x40055B4C
     */
    if (!encode_arm_branch(STOCK_FPGA_CALL_SITE,
                           DPOSLAB_FPGA_VENEER,
                           1U,
                           &fpga_branch))
        return 0;

    /*
     * Internal Phase A hook:
     *
     *     0x400491A8: BL 0x40049094
     *              -> BL 0x40055ADC
     */
    /*
     * Early CSI/init checkpoint:
     *
     *     0x40049194: BL 0x400660C8
     *              -> BL 0x40055AE4
     */
    if (!encode_arm_branch(STOCK_FPGA_EARLY_SITE,
                           DPOSLAB_FPGA_EARLY_VENEER,
                           1U,
                           &early_branch))
        return 0;

    /*
     * CSI clock/reset checkpoint:
     *
     *     0x400660D4: BL 0x40065F48
     *              -> BL 0x40055AEC
     */
    if (!encode_arm_branch(STOCK_CSI_CLOCK_RESET_SITE,
                           DPOSLAB_CSI_CLOCK_RESET_VENEER,
                           1U,
                           &csi_clock_reset_branch))
        return 0;

    /*
     * CSI controller-base binding checkpoint:
     *
     *     0x400660E4: BL 0x400658BC
     *              -> BL 0x40055AF4
     */
    if (!encode_arm_branch(STOCK_CSI_BASE_BIND_SITE,
                           DPOSLAB_CSI_BASE_BIND_VENEER,
                           1U,
                           &csi_base_bind_branch))
        return 0;

    /*
     * CSI enable/reset stage A checkpoint:
     *
     *     0x400660EC: BL 0x40065C68
     *              -> BL 0x40055AFC
     */
    if (!encode_arm_branch(STOCK_CSI_ENABLE_A_SITE,
                           DPOSLAB_CSI_ENABLE_A_VENEER,
                           1U,
                           &csi_enable_a_branch))
        return 0;

    /*
     * CSI enable/reset stage B checkpoint:
     *
     *     0x400660F4: BL 0x40065C64
     *              -> BL 0x40055B04
     */
    if (!encode_arm_branch(STOCK_CSI_ENABLE_B_SITE,
                           DPOSLAB_CSI_ENABLE_B_VENEER,
                           1U,
                           &csi_enable_b_branch))
        return 0;

    /*
     * CSI descriptor/configuration checkpoint:
     *
     *     0x400660FC: BL 0x40065FA8
     *              -> BL 0x40055B0C
     */
    if (!encode_arm_branch(STOCK_CSI_CONFIG_8000_SITE,
                           DPOSLAB_CSI_CONFIG_8000_VENEER,
                           1U,
                           &csi_config_8000_branch))
        return 0;

    /*
     * Final CSI PE-pinmux checkpoint:
     *
     *     0x40066060: BL 0x40065EF8
     *              -> BL 0x40055B14
     */
    if (!encode_arm_branch(STOCK_CSI_PINMUX_SITE,
                           DPOSLAB_CSI_PINMUX_VENEER,
                           1U,
                           &csi_pinmux_branch))
        return 0;

    if (!encode_arm_branch(STOCK_SMEC_ECHO68_SITE,
                           DPOSLAB_SMEC_ECHO68_VENEER,
                           1U,
                           &smec_echo68_branch))
        return 0;

    if (!encode_arm_branch(STOCK_SMEC_VALID67_SITE,
                           DPOSLAB_SMEC_VALID67_VENEER,
                           1U,
                           &smec_valid67_branch))
        return 0;

    if (!encode_arm_branch(STOCK_SMEC_JOB_START_SITE,
                           DPOSLAB_SMEC_JOB_START_VENEER, 1U,
                           &smec_job_start_branch))
        return 0;
    if (!encode_arm_branch(STOCK_SMEC_POLL65_SITE,
                           DPOSLAB_SMEC_POLL65_VENEER, 1U,
                           &smec_poll65_branch))
        return 0;

    if (!encode_arm_branch(STOCK_FPGA_PHASE_A_SITE,
                           DPOSLAB_FPGA_PHASE_A_VENEER,
                           1U,
                           &phase_a_branch))
        return 0;

    /*
     * Screenshot post-save hook:
     *
     *     0x400350F0: B 0x40034284
     *              -> B 0x40055AD4
     */
    if (!encode_arm_branch(STOCK_SCREENSHOT_SITE,
                           DPOSLAB_SCREENSHOT_VENEER,
                           0U,
                           &screenshot_branch))
        return 0;

    /*
     * Install absolute veneers before making their call sites reachable.
     */
    install_absolute_veneer(
        DPOSLAB_FPGA_VENEER,
        fpga_hook_address);

    install_absolute_veneer(
        DPOSLAB_FPGA_EARLY_VENEER,
        early_hook_address);

    install_absolute_veneer(
        DPOSLAB_CSI_CLOCK_RESET_VENEER,
        csi_clock_reset_hook_address);

    install_absolute_veneer(
        DPOSLAB_CSI_BASE_BIND_VENEER,
        csi_base_bind_hook_address);

    install_absolute_veneer(
        DPOSLAB_CSI_ENABLE_A_VENEER,
        csi_enable_a_hook_address);

    install_absolute_veneer(
        DPOSLAB_CSI_ENABLE_B_VENEER,
        csi_enable_b_hook_address);

    install_absolute_veneer(
        DPOSLAB_CSI_CONFIG_8000_VENEER,
        csi_config_8000_hook_address);

    install_absolute_veneer(
        DPOSLAB_CSI_PINMUX_VENEER,
        csi_pinmux_hook_address);

    install_absolute_veneer(
        DPOSLAB_SMEC_ECHO68_VENEER,
        smec_echo68_hook_address);

    install_absolute_veneer(
        DPOSLAB_SMEC_VALID67_VENEER,
        smec_valid67_hook_address);
    install_absolute_veneer(DPOSLAB_SMEC_JOB_START_VENEER,
                            smec_job_start_hook_address);
    install_absolute_veneer(DPOSLAB_SMEC_POLL65_VENEER,
                            smec_poll65_hook_address);

    install_absolute_veneer(
        DPOSLAB_FPGA_PHASE_A_VENEER,
        phase_a_hook_address);

    install_absolute_veneer(
        DPOSLAB_SCREENSHOT_VENEER,
        screenshot_hook_address);

    /*
     * Apply all stock call-site patches.
     */
    *fpga_site = fpga_branch;
    *early_site = early_branch;
    *csi_clock_reset_site = csi_clock_reset_branch;
    *csi_base_bind_site = csi_base_bind_branch;
    *csi_enable_a_site = csi_enable_a_branch;
    *csi_enable_b_site = csi_enable_b_branch;
    *csi_config_8000_site = csi_config_8000_branch;
    *csi_pinmux_site = csi_pinmux_branch;
    *smec_echo68_site = smec_echo68_branch;
    *smec_valid67_site = smec_valid67_branch;
    *smec_job_start_site = smec_job_start_branch;
    *smec_poll65_site = smec_poll65_branch;
    *phase_a_site = phase_a_branch;
    *screenshot_site = screenshot_branch;

    /*
     * Publish modified instructions and veneers to instruction fetch.
     */
    cache_clean_range(DPOSLAB_FPGA_VENEER, 8U);
    cache_clean_range(DPOSLAB_FPGA_EARLY_VENEER, 8U);
    cache_clean_range(DPOSLAB_CSI_CLOCK_RESET_VENEER, 8U);
    cache_clean_range(DPOSLAB_CSI_BASE_BIND_VENEER, 8U);
    cache_clean_range(DPOSLAB_CSI_ENABLE_A_VENEER, 8U);
    cache_clean_range(DPOSLAB_CSI_ENABLE_B_VENEER, 8U);
    cache_clean_range(DPOSLAB_CSI_CONFIG_8000_VENEER, 8U);
    cache_clean_range(DPOSLAB_CSI_PINMUX_VENEER, 8U);
    cache_clean_range(DPOSLAB_SMEC_ECHO68_VENEER, 8U);
    cache_clean_range(DPOSLAB_SMEC_VALID67_VENEER, 8U);
    cache_clean_range(DPOSLAB_SMEC_JOB_START_VENEER, 8U);
    cache_clean_range(DPOSLAB_SMEC_POLL65_VENEER, 8U);
    cache_clean_range(DPOSLAB_FPGA_PHASE_A_VENEER, 8U);
    cache_clean_range(DPOSLAB_SCREENSHOT_VENEER, 8U);

    cache_clean_range(STOCK_FPGA_CALL_SITE, 4U);
    cache_clean_range(STOCK_FPGA_EARLY_SITE, 4U);
    cache_clean_range(STOCK_CSI_CLOCK_RESET_SITE, 4U);
    cache_clean_range(STOCK_CSI_BASE_BIND_SITE, 4U);
    cache_clean_range(STOCK_CSI_ENABLE_A_SITE, 4U);
    cache_clean_range(STOCK_CSI_ENABLE_B_SITE, 4U);
    cache_clean_range(STOCK_CSI_CONFIG_8000_SITE, 4U);
    cache_clean_range(STOCK_CSI_PINMUX_SITE, 4U);
    cache_clean_range(STOCK_SMEC_ECHO68_SITE, 4U);
    cache_clean_range(STOCK_SMEC_VALID67_SITE, 4U);
    cache_clean_range(STOCK_SMEC_JOB_START_SITE, 4U);
    cache_clean_range(STOCK_SMEC_POLL65_SITE, 4U);
    cache_clean_range(STOCK_FPGA_PHASE_A_SITE, 4U);
    cache_clean_range(STOCK_SCREENSHOT_SITE, 4U);

    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    /*
     * Verify both patched sites and all absolute veneers.
     */
    return
        *fpga_site == fpga_branch &&
        *early_site == early_branch &&
        *csi_clock_reset_site == csi_clock_reset_branch &&
        *csi_base_bind_site == csi_base_bind_branch &&
        *csi_enable_a_site == csi_enable_a_branch &&
        *csi_enable_b_site == csi_enable_b_branch &&
        *csi_config_8000_site == csi_config_8000_branch &&
        *csi_pinmux_site == csi_pinmux_branch &&
        *smec_echo68_site == smec_echo68_branch &&
        *smec_valid67_site == smec_valid67_branch &&
        *smec_job_start_site == smec_job_start_branch &&
        *smec_poll65_site == smec_poll65_branch &&
        *phase_a_site == phase_a_branch &&
        *screenshot_site == screenshot_branch &&

        fpga_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        fpga_veneer[1] == fpga_hook_address &&

        early_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        early_veneer[1] == early_hook_address &&

        csi_clock_reset_veneer[0] ==
            ARM_LDR_PC_PC_MINUS_4 &&
        csi_clock_reset_veneer[1] ==
            csi_clock_reset_hook_address &&

        csi_base_bind_veneer[0] ==
            ARM_LDR_PC_PC_MINUS_4 &&
        csi_base_bind_veneer[1] ==
            csi_base_bind_hook_address &&

        csi_enable_a_veneer[0] ==
            ARM_LDR_PC_PC_MINUS_4 &&
        csi_enable_a_veneer[1] ==
            csi_enable_a_hook_address &&

        csi_enable_b_veneer[0] ==
            ARM_LDR_PC_PC_MINUS_4 &&
        csi_enable_b_veneer[1] ==
            csi_enable_b_hook_address &&

        csi_config_8000_veneer[0] ==
            ARM_LDR_PC_PC_MINUS_4 &&
        csi_config_8000_veneer[1] ==
            csi_config_8000_hook_address &&

        csi_pinmux_veneer[0] ==
            ARM_LDR_PC_PC_MINUS_4 &&
        csi_pinmux_veneer[1] ==
            csi_pinmux_hook_address &&

        smec_echo68_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        smec_echo68_veneer[1] == smec_echo68_hook_address &&

        smec_valid67_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        smec_valid67_veneer[1] == smec_valid67_hook_address &&
        smec_job_start_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        smec_job_start_veneer[1] == smec_job_start_hook_address &&
        smec_poll65_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        smec_poll65_veneer[1] == smec_poll65_hook_address &&

        phase_a_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        phase_a_veneer[1] == phase_a_hook_address &&

        screenshot_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        screenshot_veneer[1] == screenshot_hook_address;
}

/*
 * Minimal stock runtime Stage 1:
 *
 *   EXE2 startup -> sys_clock_init -> sys_mmu_init
 *   -> hook at 0x40034710 before System_Init -> stock FPGA_Start -> DPOSLAB monitor -> stop
 *
 * No application init and no internal FPGA/CSI/SMEC hooks are installed.
 */
#define STOCK_MINIMAL_MAIN_SITE       0x40034774U
#define STOCK_MINIMAL_MAIN_ORIGINAL   0xEB0023CCU /* bl Win_OpenMainBank */

#define ARM_NOP                         0xE1A00000U
#define STOCK_GUI_INIT_SITE             0x4003473CU
#define STOCK_GUI_INIT_ORIGINAL         0xEBFFFF67U
#define STOCK_GT911_INIT_SITE           0x40034740U
#define STOCK_GT911_INIT_ORIGINAL       0xEB0054CBU
#define STOCK_WIN_INIT_SITE             0x40034744U
#define STOCK_WIN_INIT_ORIGINAL         0xEB002342U
#define STOCK_UI_COLOR_INIT_SITE        0x40034748U
#define STOCK_UI_COLOR_INIT_ORIGINAL    0xEBFF94E9U
#define STOCK_FATFS_INIT_SITE           0x4003474CU
#define STOCK_FATFS_INIT_ORIGINAL       0xEB00550BU
#define STOCK_FACTORY_CHECK_SITE         0x4003476CU
#define STOCK_FACTORY_CHECK_ORIGINAL     0xEB00755FU
#define STOCK_SYS_CONFIG_EXEC_SITE       0x40034770U
#define STOCK_SYS_CONFIG_EXEC_ORIGINAL   0xEB0027E4U
#define STOCK_CHECK_UPDATE_SITE          0x40034768U
#define STOCK_CHECK_UPDATE_ORIGINAL      0xEB006A71U
#define STOCK_GEN_INIT_SITE               0x40034764U
#define STOCK_GEN_INIT_ORIGINAL           0xEB00627EU
#define STOCK_HARDCOST_INIT_SITE          0x40034760U
#define STOCK_HARDCOST_INIT_ORIGINAL      0xEB0029EDU
#define STOCK_VERTICAL_INIT_SITE          0x4003475CU
#define STOCK_VERTICAL_INIT_ORIGINAL      0xEB00576AU
#define STOCK_HORIZONTAL_INIT_SITE        0x40034758U
#define STOCK_HORIZONTAL_INIT_ORIGINAL    0xEB0017B3U
#define STOCK_POWER_INIT_SITE             0x40034754U
#define STOCK_POWER_INIT_ORIGINAL         0xEB00568FU
#define STOCK_SMEC_INIT_SITE              0x40034750U
#define STOCK_SMEC_INIT_ORIGINAL          0xEB005681U
#define STOCK_LANGUAGE_INIT_SITE          0x40034734U
#define STOCK_LANGUAGE_INIT_ORIGINAL      0xEB004418U
#define STOCK_GET_SYS_SAVED_INFO_SITE     0x40034730U
#define STOCK_GET_SYS_SAVED_INFO_ORIGINAL 0xEB004017U
#define STOCK_SPI_FLASH_INIT_SITE         0x4003472CU
#define STOCK_SPI_FLASH_INIT_ORIGINAL     0xEB003B50U
#define STOCK_MEMORY_TYPE_MOV_SITE         0x40034714U
#define STOCK_MEMORY_TYPE_MOV_ORIGINAL     0xE3A00001U
#define STOCK_MEMORY_TYPE_LDR_SITE         0x40034718U
#define STOCK_MEMORY_TYPE_LDR_ORIGINAL     0xE59F1078U
#define STOCK_MEMORY_TYPE_STR_SITE         0x4003471CU
#define STOCK_MEMORY_TYPE_STR_ORIGINAL     0xE5810000U
#define STOCK_ALL_INFO_INIT_SITE           0x40034720U
#define STOCK_ALL_INFO_INIT_ORIGINAL       0xEB002A21U


extern void dposlab_minimal_stock_fpga_hook(void);
extern void dposlab_smec_matrix_init(void);
extern void dposlab_custom_fpga_start(void);

int dposlab_install_minimal_stock_fpga_hook(void)
{
    volatile uint32_t *custom_fpga_site;
    volatile uint32_t *smec_matrix_site;
    volatile uint32_t *gui_site;
    volatile uint32_t *gt911_site;
    volatile uint32_t *win_site;
    volatile uint32_t *ui_color_site;
    volatile uint32_t *fatfs_site;
    volatile uint32_t *factory_check_site;
    volatile uint32_t *sys_config_exec_site;
    volatile uint32_t *check_update_site;
    volatile uint32_t *gen_init_site;
    volatile uint32_t *hardcost_site;
    volatile uint32_t *vertical_init_site;
    volatile uint32_t *horizontal_init_site;
    volatile uint32_t *power_init_site;
    volatile uint32_t *smec_init_site;
    volatile uint32_t *language_init_site;
    volatile uint32_t *get_sys_saved_info_site;
    volatile uint32_t *spi_flash_init_site;
    volatile uint32_t *memory_type_mov_site;
    volatile uint32_t *memory_type_ldr_site;
    volatile uint32_t *memory_type_str_site;
    volatile uint32_t *all_info_init_site;
    volatile uint32_t *site;
    volatile uint32_t *veneer;
    uint32_t hook_address;
    uint32_t custom_fpga_address;
    uint32_t custom_fpga_branch;
    uint32_t smec_matrix_address;
    uint32_t smec_matrix_branch;
    uint32_t branch;

    site = word_at(STOCK_MINIMAL_MAIN_SITE);
    custom_fpga_site = word_at(STOCK_FPGA_CALL_SITE);
    smec_matrix_site = word_at(STOCK_SMEC_MATRIX_SITE);
    gui_site = word_at(STOCK_GUI_INIT_SITE);
    gt911_site = word_at(STOCK_GT911_INIT_SITE);
    win_site = word_at(STOCK_WIN_INIT_SITE);
    ui_color_site = word_at(STOCK_UI_COLOR_INIT_SITE);
    fatfs_site = word_at(STOCK_FATFS_INIT_SITE);
    factory_check_site = word_at(STOCK_FACTORY_CHECK_SITE);
    sys_config_exec_site = word_at(STOCK_SYS_CONFIG_EXEC_SITE);
    check_update_site = word_at(STOCK_CHECK_UPDATE_SITE);
    gen_init_site = word_at(STOCK_GEN_INIT_SITE);
    hardcost_site = word_at(STOCK_HARDCOST_INIT_SITE);
    vertical_init_site = word_at(STOCK_VERTICAL_INIT_SITE);
    horizontal_init_site = word_at(STOCK_HORIZONTAL_INIT_SITE);
    power_init_site = word_at(STOCK_POWER_INIT_SITE);
    smec_init_site = word_at(STOCK_SMEC_INIT_SITE);
    language_init_site = word_at(STOCK_LANGUAGE_INIT_SITE);
    get_sys_saved_info_site = word_at(STOCK_GET_SYS_SAVED_INFO_SITE);
    spi_flash_init_site = word_at(STOCK_SPI_FLASH_INIT_SITE);
    memory_type_mov_site = word_at(STOCK_MEMORY_TYPE_MOV_SITE);
    memory_type_ldr_site = word_at(STOCK_MEMORY_TYPE_LDR_SITE);
    memory_type_str_site = word_at(STOCK_MEMORY_TYPE_STR_SITE);
    all_info_init_site = word_at(STOCK_ALL_INFO_INIT_SITE);
    veneer = word_at(DPOSLAB_FPGA_VENEER);
    hook_address =
        (uint32_t)(uintptr_t)dposlab_minimal_stock_fpga_hook;
    custom_fpga_address =
        (uint32_t)(uintptr_t)dposlab_custom_fpga_start;
    smec_matrix_address =
        (uint32_t)(uintptr_t)dposlab_smec_matrix_init;

    /* Refuse an incompatible or already-patched EXE2 image. */
    if (*site != STOCK_MINIMAL_MAIN_ORIGINAL)
        return 0;
    if (*custom_fpga_site != STOCK_FPGA_CALL_ORIGINAL)
        return 0;
    if (*smec_matrix_site != STOCK_SMEC_MATRIX_ORIGINAL)
        return 0;
    if (*gui_site != STOCK_GUI_INIT_ORIGINAL && *gui_site != ARM_NOP)
        return 0;
    if (*gt911_site != STOCK_GT911_INIT_ORIGINAL && *gt911_site != ARM_NOP)
        return 0;
    if (*win_site != STOCK_WIN_INIT_ORIGINAL && *win_site != ARM_NOP)
        return 0;
    if (*ui_color_site != STOCK_UI_COLOR_INIT_ORIGINAL && *ui_color_site != ARM_NOP)
        return 0;
    if (*fatfs_site != STOCK_FATFS_INIT_ORIGINAL && *fatfs_site != ARM_NOP)
        return 0;
    if (*factory_check_site != STOCK_FACTORY_CHECK_ORIGINAL &&
        *factory_check_site != ARM_NOP)
        return 0;
    if (*sys_config_exec_site != STOCK_SYS_CONFIG_EXEC_ORIGINAL &&
        *sys_config_exec_site != ARM_NOP)
        return 0;
    if (*check_update_site != STOCK_CHECK_UPDATE_ORIGINAL &&
        *check_update_site != ARM_NOP)
        return 0;
    if (*gen_init_site != STOCK_GEN_INIT_ORIGINAL &&
        *gen_init_site != ARM_NOP)
        return 0;
    if (*hardcost_site != STOCK_HARDCOST_INIT_ORIGINAL &&
        *hardcost_site != ARM_NOP)
        return 0;
    if (*vertical_init_site != STOCK_VERTICAL_INIT_ORIGINAL &&
        *vertical_init_site != ARM_NOP)
        return 0;
    if (*horizontal_init_site != STOCK_HORIZONTAL_INIT_ORIGINAL &&
        *horizontal_init_site != ARM_NOP)
        return 0;
    if (*power_init_site != STOCK_POWER_INIT_ORIGINAL &&
        *power_init_site != ARM_NOP)
        return 0;
    if (*smec_init_site != STOCK_SMEC_INIT_ORIGINAL &&
        *smec_init_site != ARM_NOP)
        return 0;
    if (*language_init_site != STOCK_LANGUAGE_INIT_ORIGINAL &&
        *language_init_site != ARM_NOP)
        return 0;
    if (*get_sys_saved_info_site != STOCK_GET_SYS_SAVED_INFO_ORIGINAL &&
        *get_sys_saved_info_site != ARM_NOP)
        return 0;
    if (*spi_flash_init_site != STOCK_SPI_FLASH_INIT_ORIGINAL &&
        *spi_flash_init_site != ARM_NOP)
        return 0;
    if (*memory_type_mov_site != STOCK_MEMORY_TYPE_MOV_ORIGINAL &&
        *memory_type_mov_site != ARM_NOP)
        return 0;
    if (*memory_type_ldr_site != STOCK_MEMORY_TYPE_LDR_ORIGINAL &&
        *memory_type_ldr_site != ARM_NOP)
        return 0;
    if (*memory_type_str_site != STOCK_MEMORY_TYPE_STR_ORIGINAL &&
        *memory_type_str_site != ARM_NOP)
        return 0;
    if (*all_info_init_site != STOCK_ALL_INFO_INIT_ORIGINAL &&
        *all_info_init_site != ARM_NOP)
        return 0;

    *gui_site = ARM_NOP;
    *gt911_site = ARM_NOP;
    *win_site = ARM_NOP;
    *ui_color_site = ARM_NOP;
    *fatfs_site = ARM_NOP;
    *factory_check_site = ARM_NOP;
    *check_update_site = ARM_NOP;
    *gen_init_site = ARM_NOP;
    *hardcost_site = ARM_NOP;
    *language_init_site = ARM_NOP;
    *get_sys_saved_info_site = ARM_NOP;
    *spi_flash_init_site = ARM_NOP;
    *memory_type_mov_site = ARM_NOP;
    *memory_type_ldr_site = ARM_NOP;
    *memory_type_str_site = ARM_NOP;
    *power_init_site = ARM_NOP;
    *vertical_init_site = ARM_NOP;

    if (!encode_arm_branch(STOCK_FPGA_CALL_SITE,
                           DPOSLAB_CUSTOM_FPGA_VENEER,
                           1U,
                           &custom_fpga_branch))
        return 0;
    install_absolute_veneer(DPOSLAB_CUSTOM_FPGA_VENEER,
                            custom_fpga_address);
    *custom_fpga_site = custom_fpga_branch;

    if (!encode_arm_branch(STOCK_SMEC_MATRIX_SITE,
                           DPOSLAB_SMEC_MATRIX_VENEER,
                           1U,
                           &smec_matrix_branch))
        return 0;
    install_absolute_veneer(DPOSLAB_SMEC_MATRIX_VENEER,
                            smec_matrix_address);
    *smec_matrix_site = smec_matrix_branch;

    if (!encode_arm_branch(STOCK_MINIMAL_MAIN_SITE,
                           DPOSLAB_FPGA_VENEER,
                           0U,
                           &branch))
        return 0;

    install_absolute_veneer(DPOSLAB_FPGA_VENEER, hook_address);
    *site = branch;

    cache_clean_range(DPOSLAB_FPGA_VENEER, 8U);
    cache_clean_range(DPOSLAB_SMEC_MATRIX_VENEER, 8U);
    cache_clean_range(STOCK_SMEC_MATRIX_SITE, 4U);
    cache_clean_range(DPOSLAB_CUSTOM_FPGA_VENEER, 8U);
    cache_clean_range(STOCK_FPGA_CALL_SITE, 4U);
    cache_clean_range(STOCK_GUI_INIT_SITE, 4U);
    cache_clean_range(STOCK_GT911_INIT_SITE, 4U);
    cache_clean_range(STOCK_WIN_INIT_SITE, 4U);
    cache_clean_range(STOCK_UI_COLOR_INIT_SITE, 4U);
    cache_clean_range(STOCK_FATFS_INIT_SITE, 4U);
    cache_clean_range(STOCK_FACTORY_CHECK_SITE, 4U);
    cache_clean_range(STOCK_SYS_CONFIG_EXEC_SITE, 4U);
    cache_clean_range(STOCK_CHECK_UPDATE_SITE, 4U);
    cache_clean_range(STOCK_GEN_INIT_SITE, 4U);
    cache_clean_range(STOCK_HARDCOST_INIT_SITE, 4U);
    cache_clean_range(STOCK_VERTICAL_INIT_SITE, 4U);
    cache_clean_range(STOCK_HORIZONTAL_INIT_SITE, 4U);
    cache_clean_range(STOCK_POWER_INIT_SITE, 4U);
    cache_clean_range(STOCK_SMEC_INIT_SITE, 4U);
    cache_clean_range(STOCK_LANGUAGE_INIT_SITE, 4U);
    cache_clean_range(STOCK_GET_SYS_SAVED_INFO_SITE, 4U);
    cache_clean_range(STOCK_SPI_FLASH_INIT_SITE, 4U);
    cache_clean_range(STOCK_MEMORY_TYPE_MOV_SITE, 4U);
    cache_clean_range(STOCK_MEMORY_TYPE_LDR_SITE, 4U);
    cache_clean_range(STOCK_MEMORY_TYPE_STR_SITE, 4U);
    cache_clean_range(STOCK_ALL_INFO_INIT_SITE, 4U);
    cache_clean_range(STOCK_MINIMAL_MAIN_SITE, 4U);
    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    return
        *site == branch &&
        veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        veneer[1] == hook_address;
}
