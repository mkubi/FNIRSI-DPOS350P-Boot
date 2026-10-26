#include <stdint.h>
#include "runtime_patch.h"
#include "../../arch/r11/cache.h"

#define STOCK_FPGA_CALL_SITE          0x40034738U
#define STOCK_FPGA_CALL_ORIGINAL      0xEB005292U
#define DPOSLAB_FPGA_VENEER           0x40055B4CU

#define STOCK_SCREENSHOT_SITE         0x400350F0U
#define STOCK_SCREENSHOT_ORIGINAL     0xEAFFFC63U
#define DPOSLAB_SCREENSHOT_VENEER     0x40055AD4U

#define ARM_LDR_PC_PC_MINUS_4         0xE51FF004U

extern void dposlab_fpga_start_hook(void);
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

    if (displacement < -0x02000000 ||
        displacement >  0x01FFFFFC)
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
    volatile uint32_t *veneer = word_at(veneer_address);

    veneer[0] = ARM_LDR_PC_PC_MINUS_4;
    veneer[1] = target_address;
}

int dposlab_install_fpga_hook(void)
{
    volatile uint32_t *fpga_site;
    volatile uint32_t *screenshot_site;
    volatile uint32_t *fpga_veneer;
    volatile uint32_t *screenshot_veneer;

    uint32_t fpga_hook_address;
    uint32_t screenshot_hook_address;
    uint32_t fpga_branch;
    uint32_t screenshot_branch;

    fpga_site = word_at(STOCK_FPGA_CALL_SITE);
    screenshot_site = word_at(STOCK_SCREENSHOT_SITE);

    fpga_veneer = word_at(DPOSLAB_FPGA_VENEER);
    screenshot_veneer = word_at(DPOSLAB_SCREENSHOT_VENEER);

    fpga_hook_address =
        (uint32_t)(uintptr_t)dposlab_fpga_start_hook;

    screenshot_hook_address =
        (uint32_t)(uintptr_t)dposlab_screenshot_hook;

    /*
     * Validate the complete patch set before modifying EXE2.
     */
    if (*fpga_site != STOCK_FPGA_CALL_ORIGINAL)
        return 0;

    if (*screenshot_site != STOCK_SCREENSHOT_ORIGINAL)
        return 0;

    if (!encode_arm_branch(STOCK_FPGA_CALL_SITE,
                           DPOSLAB_FPGA_VENEER,
                           1U,
                           &fpga_branch))
        return 0;

    if (!encode_arm_branch(STOCK_SCREENSHOT_SITE,
                           DPOSLAB_SCREENSHOT_VENEER,
                           0U,
                           &screenshot_branch))
        return 0;

    install_absolute_veneer(
        DPOSLAB_FPGA_VENEER,
        fpga_hook_address);

    install_absolute_veneer(
        DPOSLAB_SCREENSHOT_VENEER,
        screenshot_hook_address);

    *fpga_site = fpga_branch;
    *screenshot_site = screenshot_branch;

    cache_clean_range(DPOSLAB_FPGA_VENEER, 8U);
    cache_clean_range(DPOSLAB_SCREENSHOT_VENEER, 8U);

    cache_clean_range(STOCK_FPGA_CALL_SITE, 4U);
    cache_clean_range(STOCK_SCREENSHOT_SITE, 4U);

    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    return
        *fpga_site == fpga_branch &&
        *screenshot_site == screenshot_branch &&

        fpga_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        fpga_veneer[1] == fpga_hook_address &&

        screenshot_veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
        screenshot_veneer[1] == screenshot_hook_address;
}
