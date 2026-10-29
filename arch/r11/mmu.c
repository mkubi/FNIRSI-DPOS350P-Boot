#include "mmu.h"
#include "cache.h"

#define SECTION_COUNT               4096U
#define SECTION_SIZE                0x00100000U

#define DRAM_FIRST_SECTION          0x400U
#define DRAM_LAST_SECTION           0x47FU

#define DESC_SECTION                (2U << 0)
#define DESC_B                      (1U << 2)
#define DESC_C                      (1U << 3)
#define DESC_TEX1                   (1U << 12)
#define DESC_AP_FULL_ACCESS         (3U << 10)
#define DESC_SHARED                 (1U << 16)

#define DESC_DEVICE_SHARED          (DESC_SECTION | DESC_B | \
                                     DESC_AP_FULL_ACCESS | DESC_SHARED)
#define DESC_NORMAL_WBWA_SHARED     (DESC_SECTION | DESC_B | DESC_C | \
                                     DESC_TEX1 | DESC_AP_FULL_ACCESS | \
                                     DESC_SHARED)

#define SCTLR_MMU_ENABLE            (1U << 0)
#define SCTLR_DCACHE_ENABLE         (1U << 2)
#define SCTLR_BRANCH_ENABLE         (1U << 11)
#define SCTLR_ICACHE_ENABLE         (1U << 12)

/* ARM short-descriptor L1 table: 4096 entries, 16 KiB aligned. */
static uint32_t l1_table[SECTION_COUNT] __attribute__((aligned(16384)));

static uint32_t read_sctlr(void)
{
    uint32_t value;
    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (value));
    return value;
}

static uint32_t read_ttbr0(void)
{
    uint32_t value;
    __asm__ volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (value));
    return value;
}

static uint32_t read_dacr(void)
{
    uint32_t value;
    __asm__ volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r" (value));
    return value;
}

static void invalidate_tlb_all(void)
{
    uint32_t zero = 0U;
    __asm__ volatile (
        "mcr p15, 0, %0, c8, c7, 0"
        :
        : "r" (zero)
        : "memory"
    );
}

static void invalidate_dcache_all(void)
{
    uint32_t clidr;
    uint32_t level;

    __asm__ volatile ("mrc p15, 1, %0, c0, c0, 1" : "=r" (clidr));

    for (level = 0U; level < 7U; ++level) {
        uint32_t cache_type = (clidr >> (level * 3U)) & 7U;
        uint32_t csselr;
        uint32_t ccsidr;
        uint32_t line_shift;
        uint32_t ways;
        uint32_t sets;
        uint32_t way_shift;
        uint32_t way;
        uint32_t set;

        if (cache_type < 2U)
            continue;

        csselr = level << 1;
        __asm__ volatile (
            "mcr p15, 2, %0, c0, c0, 0\n"
            "isb"
            :
            : "r" (csselr)
            : "memory"
        );
        __asm__ volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));

        line_shift = (ccsidr & 7U) + 4U;
        ways = ((ccsidr >> 3) & 0x3FFU) + 1U;
        sets = ((ccsidr >> 13) & 0x7FFFU) + 1U;
        way_shift = ways > 1U ? (uint32_t)__builtin_clz(ways - 1U) : 31U;

        for (way = 0U; way < ways; ++way) {
            for (set = 0U; set < sets; ++set) {
                uint32_t operand = (level << 1) |
                                   (set << line_shift) |
                                   (way << way_shift);
                __asm__ volatile (
                    "mcr p15, 0, %0, c7, c6, 2"
                    :
                    : "r" (operand)
                    : "memory"
                );
            }
        }
    }

    {
        uint32_t zero = 0U;
        __asm__ volatile (
            "mcr p15, 2, %0, c0, c0, 0\n"
            "isb"
            :
            : "r" (zero)
            : "memory"
        );
    }
    cache_dsb();
}

void mmu_enable_identity_dram_cache(struct mmu_state *state)
{
    uint32_t section;
    uint32_t sctlr;
    uint32_t table_address = (uint32_t)(uintptr_t)l1_table;
    uint32_t zero = 0U;
    uint32_t domain_client = 1U;

    if (state != 0)
        state->sctlr_before = read_sctlr();

    for (section = 0U; section < SECTION_COUNT; ++section)
        l1_table[section] = section * SECTION_SIZE | DESC_DEVICE_SHARED;

    for (section = DRAM_FIRST_SECTION;
         section <= DRAM_LAST_SECTION;
         ++section) {
        l1_table[section] = section * SECTION_SIZE |
                            DESC_NORMAL_WBWA_SHARED;
    }

    cache_dsb();
    invalidate_dcache_all();
    cache_invalidate_icache_all();
    invalidate_tlb_all();
    cache_invalidate_branch_predictor();

    /* Use TTBR0 for the complete 4 GiB short-descriptor address space. */
    __asm__ volatile (
        "mcr p15, 0, %0, c2, c0, 2\n"  /* TTBCR = 0 */
        "mcr p15, 0, %1, c2, c0, 0\n"  /* TTBR0 */
        "mcr p15, 0, %2, c3, c0, 0\n"  /* DACR: domain 0 client */
        "dsb sy\n"
        "isb"
        :
        : "r" (zero), "r" (table_address), "r" (domain_client)
        : "memory"
    );

    invalidate_tlb_all();
    cache_dsb();
    cache_isb();

    sctlr = read_sctlr();
    sctlr |= SCTLR_MMU_ENABLE |
             SCTLR_DCACHE_ENABLE |
             SCTLR_BRANCH_ENABLE |
             SCTLR_ICACHE_ENABLE;
    __asm__ volatile (
        "mcr p15, 0, %0, c1, c0, 0\n"
        "dsb sy\n"
        "isb"
        :
        : "r" (sctlr)
        : "memory"
    );

    if (state != 0) {
        state->sctlr_after = read_sctlr();
        state->ttbr0 = read_ttbr0();
        state->dacr = read_dacr();
    }
}


static void clean_invalidate_dcache_all(void)
{
    uint32_t clidr;
    uint32_t level;

    if (!(read_sctlr() & SCTLR_DCACHE_ENABLE))
        return;

    __asm__ volatile ("mrc p15, 1, %0, c0, c0, 1" : "=r" (clidr));

    for (level = 0U; level < 7U; ++level) {
        uint32_t cache_type = (clidr >> (level * 3U)) & 7U;
        uint32_t csselr;
        uint32_t ccsidr;
        uint32_t line_shift;
        uint32_t ways;
        uint32_t sets;
        uint32_t way_shift;
        uint32_t way;
        uint32_t set;

        if (cache_type < 2U)
            continue;

        csselr = level << 1;
        __asm__ volatile (
            "mcr p15, 2, %0, c0, c0, 0\n"
            "isb"
            :
            : "r" (csselr)
            : "memory"
        );
        __asm__ volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));

        line_shift = (ccsidr & 7U) + 4U;
        ways = ((ccsidr >> 3) & 0x3FFU) + 1U;
        sets = ((ccsidr >> 13) & 0x7FFFU) + 1U;
        way_shift = ways > 1U ? (uint32_t)__builtin_clz(ways - 1U) : 31U;

        for (way = 0U; way < ways; ++way) {
            for (set = 0U; set < sets; ++set) {
                uint32_t operand = (level << 1) |
                                   (set << line_shift) |
                                   (way << way_shift);
                __asm__ volatile (
                    "mcr p15, 0, %0, c7, c14, 2"
                    :
                    : "r" (operand)
                    : "memory"
                );
            }
        }
    }

    {
        uint32_t zero = 0U;
        __asm__ volatile (
            "mcr p15, 2, %0, c0, c0, 0\n"
            "isb"
            :
            : "r" (zero)
            : "memory"
        );
    }
    cache_dsb();
}

void mmu_disable_identity_cache(void)
{
    uint32_t sctlr;

    /* Publish every dirty DRAM line before removing cacheable mappings. */
    clean_invalidate_dcache_all();
    cache_dsb();

    sctlr = read_sctlr();
    sctlr &= ~(SCTLR_MMU_ENABLE |
               SCTLR_DCACHE_ENABLE |
               SCTLR_BRANCH_ENABLE |
               SCTLR_ICACHE_ENABLE);
    __asm__ volatile (
        "mcr p15, 0, %0, c1, c0, 0\n"
        "dsb sy\n"
        "isb"
        :
        : "r" (sctlr)
        : "memory"
    );

    cache_invalidate_icache_all();
    cache_invalidate_branch_predictor();
    invalidate_tlb_all();
    cache_dsb();
    cache_isb();
}
