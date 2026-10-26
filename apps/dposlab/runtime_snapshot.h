#ifndef DPOSLAB_RUNTIME_SNAPSHOT_H
#define DPOSLAB_RUNTIME_SNAPSHOT_H

#include <stdint.h>

struct dposlab_cpu_frame {
    uint32_t r[13];
    uint32_t lr;
};

struct dposlab_runtime_snapshot {
    uint32_t r[13];
    uint32_t lr;
    uint32_t cpsr;

    /*
     * stock_sp is the SP at the original patched call-site.
     * exec_sp is the SP actually seen by FPGA_Start after the wrapper
     * has saved the original stock context.
     */
    uint32_t stock_sp;
    uint32_t exec_sp;

    uint32_t pe_cfg0;
    uint32_t pe_cfg1;
    uint32_t pe_data;
    uint32_t pe_pull0;

    uint32_t ccu_gate0;
    uint32_t ccu_reset0;

    uint32_t csi_if_cfg;
    uint32_t csi_mode;
    uint32_t csi_hsize;
    uint32_t csi_destination;
    uint32_t csi_irq_enable;
    uint32_t csi_status;
};

extern volatile struct dposlab_runtime_snapshot dposlab_fpga_before;
extern volatile struct dposlab_runtime_snapshot dposlab_fpga_after;

void dposlab_capture_before(uint32_t cpsr,
                            const struct dposlab_cpu_frame *frame);
void dposlab_capture_after(uint32_t cpsr,
                           const struct dposlab_cpu_frame *frame);

uint32_t dposlab_gpr_changed_mask(void);

#endif
