/*
 * V3s/S3 clock initialization adapted from xboot/xfel:
 * payloads/v3s_s3/v3s-ddr/source/sys-clock.c
 *
 * Copyright (c) 2007-2021 Jianjun Jiang <8192542@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include "clock.h"
#include "regs.h"

static inline void short_delay(int loops)
{
    __asm__ volatile (
        "1:\n"
        "subs %0, %1, #1\n"
        "bne 1b"
        : "=r" (loops)
        : "0" (loops)
    );
}

static void clock_set_pll_cpu(uint32_t clock_hz)
{
    int p = 0;
    int k = 1;
    int m = 1;
    uint32_t value;

    if (clock_hz > 1152000000U) {
        k = 2;
    } else if (clock_hz > 768000000U) {
        k = 3;
        m = 2;
    }

    /* Switch CPU to OSC24M while changing PLL_CPU. */
    value = (2U << 0) | (1U << 8) | (1U << 16);
    mmio_write32(V3S_CCU_BASE + CCU_CPU_AXI_CFG, value);

    /* PLL_CPU = ((24 MHz * N * K) >> P) / M. */
    value = 1U << 31;
    value |= ((uint32_t)(p & 0x3) << 16);
    value |= ((((clock_hz / (24000000U * (uint32_t)k / (uint32_t)m)) - 1U)
               & 0x1FU) << 8);
    value |= ((uint32_t)((k - 1) & 0x3) << 4);
    value |= ((uint32_t)((m - 1) & 0x3) << 0);
    mmio_write32(V3S_CCU_BASE + CCU_PLL_CPU_CTRL, value);
    short_delay(200);

    /* Switch CPU clock source back to PLL_CPU. */
    value = (2U << 0) | (1U << 8) | (2U << 16);
    mmio_write32(V3S_CCU_BASE + CCU_CPU_AXI_CFG, value);
}

void sys_clock_init(void)
{
    clock_set_pll_cpu(1008000000U);

    /* PLL video: 396 MHz. */
    mmio_write32(V3S_CCU_BASE + CCU_PLL_VIDEO_CTRL, 0x91004107U);

    /* PLL peripheral 0: 600 MHz. */
    mmio_write32(V3S_CCU_BASE + CCU_PLL_PERIPH0_CTRL, 0x90041811U);
    while (!(mmio_read32(V3S_CCU_BASE + CCU_PLL_PERIPH0_CTRL) & (1U << 28)))
        ;

    /* AHB1 = PLL_PERIPH0 / 3, APB1 = AHB1 / 2. */
    mmio_write32(V3S_CCU_BASE + CCU_AHB_APB0_CFG, 0x00003180U);

    /* MBUS = PLL_PERIPH0 / 4. */
    mmio_write32(V3S_CCU_BASE + CCU_MBUS_CLK, 0x81000003U);
}
