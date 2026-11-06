#include <stdint.h>

#include "dposlab_abi.h"
#include "fat_diag_patch.h"
#include "cache.h"
#include "jump.h"

#define STOCK_EXE2_ENTRY       0x40000000U
#define STOCK_EXE2_ENTRY_WORD  0xE59FF018U

static void stop(void) __attribute__((noreturn));

static void stop(void)
{
    for (;;)
        __asm__ volatile("wfi");
}

int main(const dposlab_handoff_t *handoff)
{
    if (!handoff)
        stop();

    if (handoff->magic != DPOSLAB_HANDOFF_MAGIC ||
        handoff->version != DPOSLAB_HANDOFF_VERSION ||
        handoff->size != sizeof(*handoff) ||
        handoff->stage2_sp == 0U)
        stop();

    if ((handoff->flags & DPOSLAB_FLAG_EXE2_LOADED) == 0U)
        stop();

    if (*(volatile const uint32_t *)(uintptr_t)STOCK_EXE2_ENTRY !=
        STOCK_EXE2_ENTRY_WORD)
        stop();

    if (!fat_diag_install_patch())
        stop();

    cache_clean_range(
        (uintptr_t)handoff->exe2_base,
        handoff->exe2_size);

    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    cache_dsb();
    cache_isb();

    loader_jump(STOCK_EXE2_ENTRY);
}
