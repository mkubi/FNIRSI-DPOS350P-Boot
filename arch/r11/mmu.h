#ifndef R11_MMU_H
#define R11_MMU_H

#include <stdint.h>

struct mmu_state {
    uint32_t sctlr_before;
    uint32_t sctlr_after;
    uint32_t ttbr0;
    uint32_t dacr;
};

/*
 * Install a 1 MiB-section identity map:
 *   - all addresses: shared Device memory
 *   - 0x40000000..0x47FFFFFF: shared Normal WB/WA cacheable DRAM
 *
 * Then enable MMU, D-cache, I-cache, and branch prediction.
 */
void mmu_enable_identity_dram_cache(struct mmu_state *state);

/* Restore the historical Stage 2 handoff state: MMU/caches off. */
void mmu_disable_identity_cache(void);

#endif /* R11_MMU_H */
