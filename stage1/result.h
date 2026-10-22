#ifndef STAGE1_RESULT_H
#define STAGE1_RESULT_H

#include <stdint.h>

#define STAGE1_RESULT_ADDR  0x00009F80U
#define STAGE1_CLOCK_MAGIC  0x314B4C43U /* "CLK1" little-endian */
#define STAGE1_CLOCK_ENTER  0x00000001U
#define STAGE1_CLOCK_DONE   0x00000002U

typedef struct {
    uint32_t magic;
    uint32_t state;
    uint32_t image_base;
    uint32_t original_sp;
    uint32_t original_lr;

    uint32_t pll_cpu_before;
    uint32_t cpu_axi_before;
    uint32_t pll_video_before;
    uint32_t pll_periph0_before;
    uint32_t ahb_apb0_before;
    uint32_t mbus_before;

    uint32_t pll_cpu_after;
    uint32_t cpu_axi_after;
    uint32_t pll_video_after;
    uint32_t pll_periph0_after;
    uint32_t ahb_apb0_after;
    uint32_t mbus_after;

    uint32_t sentinel_a;
    uint32_t sentinel_b;
} stage1_clock_result_t;

#endif /* STAGE1_RESULT_H */
