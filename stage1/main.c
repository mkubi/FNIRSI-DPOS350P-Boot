#include <stdint.h>
#include "result.h"

#define V3S_CCU_BASE          0x01C20000U
#define V3S_DRAM_COM_BASE     0x01C62000U
#define V3S_DRAM_CTL_BASE     0x01C63000U

#define CCU_PLL_DDR0_CTRL     0x020U
#define CCU_BUS_CLK_GATE0     0x060U
#define CCU_DRAM_CFG          0x0F4U
#define CCU_MBUS_RST          0x0FCU
#define CCU_DRAM_CLK_GATE     0x100U
#define CCU_MBUS_CLK          0x15CU
#define CCU_BUS_SOFT_RST0     0x2C0U

#define DRAM_COM_CR           0x000U
#define DRAM_CTL_PGSR0        0x010U
#define DRAM_CTL_STATR        0x018U

#define PATTERN_A             0xAAAAAAAAU
#define PATTERN_B             0x55555555U

extern uint8_t __image_start;

static volatile stage1_dram_result_t *const result =
    (volatile stage1_dram_result_t *)(uintptr_t)STAGE1_RESULT_ADDR;

static uint32_t mmio_read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void barrier(void)
{
    __asm__ volatile ("dsb sy\nisb sy" ::: "memory");
}

static uint32_t read_dram_size_from_xfel_slot(void)
{
    uint32_t value;
    uint32_t address = 0x00000020U;

    /* Keep GCC 16 from treating the deliberate low-SRAM access as UB. */
    __asm__ volatile (
        "ldr %0, [%1]"
        : "=r" (value)
        : "r" (address)
        : "memory"
    );

    return value;
}

static int test_word(uint32_t address)
{
    volatile uint32_t *word = (volatile uint32_t *)(uintptr_t)address;
    uint32_t original = *word;
    int pass = 1;

    *word = PATTERN_A;
    barrier();
    if (*word != PATTERN_A)
        pass = 0;

    *word = PATTERN_B;
    barrier();
    if (*word != PATTERN_B)
        pass = 0;

    *word = original;
    barrier();

    return pass;
}

static void snapshot_controller(void)
{
    result->pll_ddr0_ctrl = mmio_read32(V3S_CCU_BASE + CCU_PLL_DDR0_CTRL);
    result->dram_cfg = mmio_read32(V3S_CCU_BASE + CCU_DRAM_CFG);
    result->dram_clk_gate = mmio_read32(V3S_CCU_BASE + CCU_DRAM_CLK_GATE);
    result->mbus_clk = mmio_read32(V3S_CCU_BASE + CCU_MBUS_CLK);
    result->mbus_reset = mmio_read32(V3S_CCU_BASE + CCU_MBUS_RST);
    result->bus_clk_gate0 = mmio_read32(V3S_CCU_BASE + CCU_BUS_CLK_GATE0);
    result->bus_soft_reset0 = mmio_read32(V3S_CCU_BASE + CCU_BUS_SOFT_RST0);

    result->dram_com_cr = mmio_read32(V3S_DRAM_COM_BASE + DRAM_COM_CR);
    result->dram_ctl_pgsr0 = mmio_read32(V3S_DRAM_CTL_BASE + DRAM_CTL_PGSR0);
    result->dram_ctl_statr = mmio_read32(V3S_DRAM_CTL_BASE + DRAM_CTL_STATR);
}

void stage1_dram_report(uint32_t original_sp, uint32_t original_lr)
{
    static const uint32_t addresses[4] = {
        DRAM_TEST_ADDR0,
        DRAM_TEST_ADDR1,
        DRAM_TEST_ADDR2,
        DRAM_TEST_ADDR3
    };
    uint32_t pass_mask = 0U;
    uint32_t fail_mask = 0U;
    unsigned int i;

    result->magic = STAGE1_DRAM_MAGIC;
    result->state = STAGE1_DRAM_ENTER;
    result->image_base = (uint32_t)(uintptr_t)&__image_start;
    result->original_sp = original_sp;
    result->original_lr = original_lr;
    result->dram_size_bytes = read_dram_size_from_xfel_slot();
    result->sentinel_a = 0xA55A5AA5U;
    result->sentinel_b = 0x5AA5A55AU;

    for (i = 0U; i < 4U; i++) {
        if (test_word(addresses[i]))
            pass_mask |= 1U << i;
        else
            fail_mask |= 1U << i;
    }

    result->test_pass_mask = pass_mask;
    result->test_fail_mask = fail_mask;
    snapshot_controller();

    result->state = STAGE1_DRAM_DONE;
    barrier();
}
