#include "cache.h"
#define SCTLR_DCACHE_ENABLE (1U << 2)
static uint32_t read_sctlr(void) { uint32_t v; __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(v)); return v; }
static uint32_t read_ctr(void) { uint32_t v; __asm__ volatile ("mrc p15, 0, %0, c0, c0, 1" : "=r"(v)); return v; }
void cache_clean_range(uintptr_t start, size_t bytes) {
    uintptr_t p, end, line; uint32_t ctr;
    if (!(read_sctlr() & SCTLR_DCACHE_ENABLE) || bytes == 0U) return;
    ctr = read_ctr(); line = 4U << ((ctr >> 16) & 0xFU);
    p = start & ~(line - 1U); end = start + bytes;
    while (p < end) { __asm__ volatile ("mcr p15, 0, %0, c7, c10, 1" : : "r"(p) : "memory"); p += line; }
    cache_dsb();
}


void cache_disable_dcache(void)
{
    uint32_t sctlr;

    /* Clean currently dirty data first, then disable SCTLR.C. */
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (sctlr));
    sctlr &= ~(1U << 2);
    __asm__ volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (sctlr) : "memory");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
}
