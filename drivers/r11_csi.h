#ifndef R11_CSI_H
#define R11_CSI_H

#include <stdint.h>
#include <stddef.h>

#define R11_CSI_BASE 0x01CB4000U
#define R11_CSI_DONE (1U << 1)

struct r11_csi_snapshot {
    uint32_t ccu_0064;
    uint32_t ccu_0100;
    uint32_t ccu_0134;
    uint32_t pe_cfg0;
    uint32_t pe_cfg1;
    uint32_t pe_data;
    uint32_t reg_0008;
    uint32_t reg_0050;
    uint32_t reg_0070;
    uint32_t reg_0074;
    uint32_t reg_0080;
    uint32_t reg_0084;
    uint32_t reg_0088;
};

struct r11_csi_init_result {
    uint32_t ccu_0064_before;
    uint32_t ccu_0064_after;
    uint32_t ccu_02c4_before;
    uint32_t ccu_02c4_after;
    uint32_t ccu_002c_before;
    uint32_t ccu_002c_after;
    uint32_t ccu_0100_before;
    uint32_t ccu_0100_after;
    uint32_t ccu_0134_before;
    uint32_t ccu_0134_after;
    uint32_t pe_cfg0_before;
    uint32_t pe_cfg0_after;
    uint32_t pe_cfg1_before;
    uint32_t pe_cfg1_after;
    uint32_t buffer_expected;
    uint32_t buffer_readback;
    int pll_ready;
    int buffer_ok;
};

struct r11_csi_config_result {
    uint32_t geo0_expected;
    uint32_t geo0_readback;
    uint32_t geo1_expected;
    uint32_t geo1_readback;
    uint32_t geo2_expected;
    uint32_t geo2_readback;
    uint32_t irq_expected;
    uint32_t irq_readback;
    uint32_t mode_readback;
    int capture_disabled;
};

void r11_csi_snapshot(struct r11_csi_snapshot *snapshot);
int r11_csi_configure_no_capture(struct r11_csi_config_result *result);
int r11_csi_prepare_no_capture(uintptr_t buffer_address,
                               struct r11_csi_init_result *result);
uint32_t r11_csi_status(void);
void r11_csi_ack(uint32_t mask);
void r11_csi_set_buffer0(uintptr_t address);
uint32_t r11_csi_buffer0(void);
void r11_csi_capture_enable(void);
void r11_csi_capture_disable(void);
uint32_t r11_csi_mode(void);
int r11_csi_wait_done(uint32_t timeout_ms, uint32_t *last_status);

#endif
