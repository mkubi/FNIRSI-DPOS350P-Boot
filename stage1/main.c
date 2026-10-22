#include <stdint.h>
#include "clock.h"
#include "regs.h"
#include "result.h"

extern uint8_t __image_start;

static volatile stage1_clock_result_t *const result =
    (volatile stage1_clock_result_t *)(uintptr_t)STAGE1_RESULT_ADDR;

static void snapshot_before(void)
{
    result->pll_cpu_before =
        mmio_read32(V3S_CCU_BASE + CCU_PLL_CPU_CTRL);
    result->cpu_axi_before =
        mmio_read32(V3S_CCU_BASE + CCU_CPU_AXI_CFG);
    result->pll_video_before =
        mmio_read32(V3S_CCU_BASE + CCU_PLL_VIDEO_CTRL);
    result->pll_periph0_before =
        mmio_read32(V3S_CCU_BASE + CCU_PLL_PERIPH0_CTRL);
    result->ahb_apb0_before =
        mmio_read32(V3S_CCU_BASE + CCU_AHB_APB0_CFG);
    result->mbus_before =
        mmio_read32(V3S_CCU_BASE + CCU_MBUS_CLK);
}

static void snapshot_after(void)
{
    result->pll_cpu_after =
        mmio_read32(V3S_CCU_BASE + CCU_PLL_CPU_CTRL);
    result->cpu_axi_after =
        mmio_read32(V3S_CCU_BASE + CCU_CPU_AXI_CFG);
    result->pll_video_after =
        mmio_read32(V3S_CCU_BASE + CCU_PLL_VIDEO_CTRL);
    result->pll_periph0_after =
        mmio_read32(V3S_CCU_BASE + CCU_PLL_PERIPH0_CTRL);
    result->ahb_apb0_after =
        mmio_read32(V3S_CCU_BASE + CCU_AHB_APB0_CFG);
    result->mbus_after =
        mmio_read32(V3S_CCU_BASE + CCU_MBUS_CLK);
}

void stage1_clock_main(uint32_t original_sp,
                       uint32_t original_lr)
{
    result->magic = STAGE1_CLOCK_MAGIC;
    result->state = STAGE1_CLOCK_ENTER;
    result->image_base = (uint32_t)(uintptr_t)&__image_start;
    result->original_sp = original_sp;
    result->original_lr = original_lr;
    result->sentinel_a = 0xA55A5AA5U;
    result->sentinel_b = 0x5AA5A55AU;

    snapshot_before();
    __asm__ volatile ("dsb sy" ::: "memory");

    sys_clock_init();

    __asm__ volatile ("dsb sy\nisb sy" ::: "memory");
    snapshot_after();

    result->state = STAGE1_CLOCK_DONE;
    __asm__ volatile ("dsb sy\nisb sy" ::: "memory");
}
