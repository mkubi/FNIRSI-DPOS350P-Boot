#ifndef R11_CACHE_H
#define R11_CACHE_H

#include <stddef.h>
#include <stdint.h>

void cache_clean_range(uintptr_t start, size_t bytes);
void cache_invalidate_icache_all(void);
void cache_invalidate_branch_predictor(void);

/* Kept for later use; not used by the first application handoff. */
void cache_disable_dcache(void);

static inline void cache_dsb(void)
{
    __asm__ volatile ("dsb sy" ::: "memory");
}

static inline void cache_isb(void)
{
    __asm__ volatile ("isb sy" ::: "memory");
}

#endif /* R11_CACHE_H */
