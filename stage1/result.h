#ifndef STAGE1_RESULT_H
#define STAGE1_RESULT_H

#include <stdint.h>

#define STAGE1_RESULT_ADDR 0x00009F80U
#define STAGE1_DRAM_MAGIC  0x314D5244U /* "DRM1" little-endian */
#define STAGE1_DRAM_ENTER  0x00000001U
#define STAGE1_DRAM_DONE   0x00000002U

#define DRAM_TEST_ADDR0    0x40000000U
#define DRAM_TEST_ADDR1    0x40001000U
#define DRAM_TEST_ADDR2    0x41000000U
#define DRAM_TEST_ADDR3    0x41001000U

typedef struct {
    uint32_t magic;
    uint32_t state;
    uint32_t image_base;
    uint32_t original_sp;
    uint32_t original_lr;
    uint32_t dram_size_bytes;
    uint32_t test_pass_mask;
    uint32_t test_fail_mask;

    uint32_t pll_ddr0_ctrl;
    uint32_t dram_cfg;
    uint32_t dram_clk_gate;
    uint32_t mbus_clk;
    uint32_t mbus_reset;
    uint32_t bus_clk_gate0;
    uint32_t bus_soft_reset0;

    uint32_t dram_com_cr;
    uint32_t dram_ctl_pgsr0;
    uint32_t dram_ctl_statr;

    uint32_t sentinel_a;
    uint32_t sentinel_b;
} stage1_dram_result_t;

_Static_assert(sizeof(stage1_dram_result_t) == 0x50U,
               "Unexpected Stage 1A.3 result size");

#endif /* STAGE1_RESULT_H */
