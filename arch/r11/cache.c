#include "cache.h"

#define SCTLR_DCACHE_ENABLE (1U << 2)

static uint32_t read_sctlr(void)
{
    uint32_t value;

    __asm__ volatile (
        "mrc p15, 0, %0, c1, c0, 0"
        : "=r" (value)
    );
    return value;
}

int cache_dcache_enabled(void)
{
    return (read_sctlr() & SCTLR_DCACHE_ENABLE) != 0U;
}

static uint32_t read_ctr(void)
{
    uint32_t value;

    __asm__ volatile (
        "mrc p15, 0, %0, c0, c0, 1"
        : "=r" (value)
    );
    return value;
}

void cache_clean_range(uintptr_t start, size_t bytes)
{
    uintptr_t address;
    uintptr_t end;
    uintptr_t line_size;
    uint32_t ctr;

    if (!(read_sctlr() & SCTLR_DCACHE_ENABLE) || bytes == 0U)
        return;

    ctr = read_ctr();
    line_size = 4U << ((ctr >> 16) & 0xFU);
    address = start & ~(line_size - 1U);
    end = start + bytes;

    while (address < end) {
        __asm__ volatile (
            "mcr p15, 0, %0, c7, c10, 1"
            :
            : "r" (address)
            : "memory"
        );
        address += line_size;
    }

    cache_dsb();
}

void cache_invalidate_range(uintptr_t start, size_t bytes)
{
    uintptr_t address;
    uintptr_t end;
    uintptr_t line_size;
    uint32_t ctr;

    if (!cache_dcache_enabled() || bytes == 0U)
        return;

    ctr = read_ctr();
    line_size = 4U << ((ctr >> 16) & 0xFU);
    address = start & ~(line_size - 1U);
    end = start + bytes;

    cache_dsb();
    while (address < end) {
        __asm__ volatile (
            "mcr p15, 0, %0, c7, c6, 1"
            :
            : "r" (address)
            : "memory"
        );
        address += line_size;
    }
    cache_dsb();
}

void cache_invalidate_icache_all(void)
{
    uint32_t zero = 0U;

    cache_dsb();
    __asm__ volatile (
        "mcr p15, 0, %0, c7, c5, 0"
        :
        : "r" (zero)
        : "memory"
    );
    cache_dsb();
    cache_isb();
}

void cache_invalidate_branch_predictor(void)
{
    uint32_t zero = 0U;

    __asm__ volatile (
        "mcr p15, 0, %0, c7, c5, 6"
        :
        : "r" (zero)
        : "memory"
    );
    cache_dsb();
    cache_isb();
}

void cache_disable_dcache(void)
{
    uint32_t sctlr;

    /*
     * This helper only disables SCTLR.C. Callers must clean every dirty
     * range first. It is deliberately not used by loader_start_image().
     */
    cache_dsb();
    __asm__ volatile (
        "mrc p15, 0, %0, c1, c0, 0"
        : "=r" (sctlr)
    );
    sctlr &= ~SCTLR_DCACHE_ENABLE;
    __asm__ volatile (
        "mcr p15, 0, %0, c1, c0, 0"
        :
        : "r" (sctlr)
        : "memory"
    );
    cache_dsb();
    cache_isb();
}
