#include <stdint.h>
#include "runtime_snapshot.h"
#include "runtime_trace.h"

#define MMIO32(address)     (*(volatile const uint32_t *)(uintptr_t)(address))

#define PE_CFG0_ADDRESS             0x01C20890U
#define PE_CFG1_ADDRESS             0x01C20894U
#define PE_DATA_ADDRESS             0x01C208A0U
#define PE_PULL0_ADDRESS            0x01C208ACU

#define CCU_GATE0_ADDRESS           0x01C20060U
#define CCU_RESET0_ADDRESS          0x01C202C0U

#define CSI_BASE                    0x01CB4000U
#define CSI_IF_CFG_ADDRESS          (CSI_BASE + 0x04U)
#define CSI_MODE_ADDRESS            (CSI_BASE + 0x08U)
#define CSI_HSIZE_ADDRESS           (CSI_BASE + 0x44U)
#define CSI_DESTINATION_ADDRESS     (CSI_BASE + 0x50U)
#define CSI_IRQ_ENABLE_ADDRESS      (CSI_BASE + 0x70U)
#define CSI_STATUS_ADDRESS          (CSI_BASE + 0x74U)

volatile struct dposlab_runtime_snapshot dposlab_fpga_before;
volatile struct dposlab_runtime_snapshot dposlab_fpga_after;

static void capture_passive_mmio(
    volatile struct dposlab_runtime_snapshot *snapshot)
{
    snapshot->pe_cfg0 = MMIO32(PE_CFG0_ADDRESS);
    snapshot->pe_cfg1 = MMIO32(PE_CFG1_ADDRESS);
    snapshot->pe_data = MMIO32(PE_DATA_ADDRESS);
    snapshot->pe_pull0 = MMIO32(PE_PULL0_ADDRESS);

    snapshot->ccu_gate0 = MMIO32(CCU_GATE0_ADDRESS);
    snapshot->ccu_reset0 = MMIO32(CCU_RESET0_ADDRESS);

    snapshot->csi_if_cfg = MMIO32(CSI_IF_CFG_ADDRESS);
    snapshot->csi_mode = MMIO32(CSI_MODE_ADDRESS);
    snapshot->csi_hsize = MMIO32(CSI_HSIZE_ADDRESS);
    snapshot->csi_destination = MMIO32(CSI_DESTINATION_ADDRESS);
    snapshot->csi_irq_enable = MMIO32(CSI_IRQ_ENABLE_ADDRESS);
    snapshot->csi_status = MMIO32(CSI_STATUS_ADDRESS);
}

static void capture_snapshot(
    volatile struct dposlab_runtime_snapshot *snapshot,
    uint32_t cpsr,
    const struct dposlab_cpu_frame *frame,
    uint32_t before)
{
    uint32_t i;

    for (i = 0U; i < 13U; ++i)
        snapshot->r[i] = frame->r[i];

    snapshot->lr = frame->lr;
    snapshot->cpsr = cpsr;

    /*
     * The frame contains r0-r12 and LR:
     *
     *     14 words = 56 bytes
     */
    snapshot->exec_sp = (uint32_t)(uintptr_t)frame;

    if (before != 0U)
        snapshot->stock_sp =
            (uint32_t)(uintptr_t)frame + sizeof(*frame);
    else
        snapshot->stock_sp = dposlab_fpga_before.stock_sp;

    capture_passive_mmio(snapshot);
}

void dposlab_capture_before(uint32_t cpsr,
                            const struct dposlab_cpu_frame *frame)
{
    capture_snapshot(&dposlab_fpga_before, cpsr, frame, 1U);

    /* SMEC retry profile: keep the before snapshot but do not consume a record. */
}

void dposlab_capture_after(uint32_t cpsr,
                           const struct dposlab_cpu_frame *frame)
{
    capture_snapshot(&dposlab_fpga_after, cpsr, frame, 0U);

    /* SMEC retry profile: keep the after snapshot but do not consume a record. */
}

void dposlab_capture_checkpoint(
    uint32_t checkpoint,
    uint32_t execution_address,
    uint32_t cpsr,
    const struct dposlab_cpu_frame *frame)
{
    struct dposlab_runtime_snapshot snapshot;

    /*
     * This experiment records only the first two startup jobs. At both
     * 0x4004A0F0 and 0x4004A0F8, stock keeps the requested ID in r7.
     * Two records per attempt fit the existing 32-record ABI even for the
     * complete stock retry sequence: Job 1 once plus Job 2 fourteen times.
     */
    if (checkpoint != DPOSLAB_CP_SMEC_AFTER_ECHO68 &&
        checkpoint != DPOSLAB_CP_SMEC_AFTER_VALID67 &&
        checkpoint != DPOSLAB_CP_SMEC_JOB2_BEFORE_START &&
        checkpoint != DPOSLAB_CP_SMEC_JOB2_AFTER_START &&
        checkpoint != DPOSLAB_CP_SMEC_JOB2_BEFORE_POLL65 &&
        checkpoint != DPOSLAB_CP_SMEC_JOB2_AFTER_POLL65)
        return;

    if ((checkpoint == DPOSLAB_CP_SMEC_AFTER_ECHO68 ||
         checkpoint == DPOSLAB_CP_SMEC_AFTER_VALID67) &&
        (frame->r[7] == 0U || frame->r[7] > 2U))
        return;

    if ((checkpoint == DPOSLAB_CP_SMEC_JOB2_BEFORE_START ||
         checkpoint == DPOSLAB_CP_SMEC_JOB2_AFTER_START ||
         checkpoint == DPOSLAB_CP_SMEC_JOB2_BEFORE_POLL65 ||
         checkpoint == DPOSLAB_CP_SMEC_JOB2_AFTER_POLL65) &&
        frame->r[7] != 2U)
        return;

    capture_snapshot(&snapshot, cpsr, frame, 1U);

    dposlab_trace_append(
        checkpoint,
        execution_address,
        &snapshot);
}

uint32_t dposlab_gpr_changed_mask(void)
{
    uint32_t i;
    uint32_t mask = 0U;

    for (i = 0U; i < 13U; ++i) {
        if (dposlab_fpga_before.r[i] != dposlab_fpga_after.r[i])
            mask |= 1U << i;
    }

    return mask;
}
