#ifndef STAGE1_REGS_H
#define STAGE1_REGS_H

#include <stdint.h>

#define V3S_CCU_BASE          0x01C20000U

#define CCU_PLL_CPU_CTRL      0x000U
#define CCU_PLL_VIDEO_CTRL    0x010U
#define CCU_PLL_PERIPH0_CTRL  0x028U
#define CCU_CPU_AXI_CFG       0x050U
#define CCU_AHB_APB0_CFG      0x054U
#define CCU_MBUS_CLK          0x15CU

static inline uint32_t mmio_read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static inline void mmio_write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

#endif /* STAGE1_REGS_H */
