#include <stdint.h>
#include "runtime_patch.h"
#include "../../arch/r11/cache.h"

#define STOCK_FPGA_CALL_SITE       0x40034738U
#define STOCK_FPGA_CALL_ORIGINAL   0xEB005292U

/*
 * Reserved near-veneer pool already used by the resident DPOSDBG
 * infrastructure:
 *
 *     0x40055AD4 .. 0x40055B54
 *
 * This PoC uses its final aligned 8-byte slot.
 */
#define DPOSLAB_FPGA_VENEER        0x40055B4CU

#define ARM_LDR_PC_PC_MINUS_4      0xE51FF004U

extern void dposlab_fpga_start_hook(void);

static volatile uint32_t *word_at(uint32_t address)
{
    return (volatile uint32_t *)(uintptr_t)address;
}

static int encode_arm_bl(uint32_t site, uint32_t target, uint32_t *opcode)
{
    int32_t displacement;
    int32_t word_offset;

    displacement = (int32_t)(target - (site + 8U));

    if ((displacement & 3) != 0)
        return 0;

    /*
     * ARM B/BL immediate range:
     *     -0x02000000 .. +0x01FFFFFC
     */
    if (displacement < -0x02000000 ||
        displacement >  0x01FFFFFC)
        return 0;

    word_offset = displacement >> 2;
    *opcode = 0xEB000000U | ((uint32_t)word_offset & 0x00FFFFFFU);
    return 1;
}

int dposlab_install_fpga_hook(void)
{
    volatile uint32_t *site;
    volatile uint32_t *veneer;
    uint32_t hook_address;
    uint32_t branch_opcode;

    site = word_at(STOCK_FPGA_CALL_SITE);
    veneer = word_at(DPOSLAB_FPGA_VENEER);
    hook_address = (uint32_t)(uintptr_t)dposlab_fpga_start_hook;

    if (*site != STOCK_FPGA_CALL_ORIGINAL)
        return 0;

    if (!encode_arm_bl(STOCK_FPGA_CALL_SITE,
                       DPOSLAB_FPGA_VENEER,
                       &branch_opcode))
        return 0;

    /*
     * Near veneer:
     *
     *   0x40055B4C: LDR PC, [PC, #-4]
     *   0x40055B50: absolute DPOSLAB hook address
     */
    veneer[0] = ARM_LDR_PC_PC_MINUS_4;
    veneer[1] = hook_address;

    /*
     * Replace:
     *
     *   BL 0x40049188
     *
     * with:
     *
     *   BL 0x40055B4C
     */
    *site = branch_opcode;

    cache_clean_range(DPOSLAB_FPGA_VENEER, 8U);
    cache_clean_range(STOCK_FPGA_CALL_SITE, 4U);
    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    return *site == branch_opcode &&
           veneer[0] == ARM_LDR_PC_PC_MINUS_4 &&
           veneer[1] == hook_address;
}
