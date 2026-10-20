#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/*
 * FNIRSI DPOS350P memory map
 *
 * DRAM: 64 MiB DDR2 SIP, base 0x40000000
 */

#define DRAM_BASE           0x40000000UL
#define DRAM_SIZE           (64U * 1024U * 1024U)   /* 64 MiB */

/* Boot manager load address (Phase 0/FEL: loaded by xfel) */
#define BOOT_MGR_ADDR       0x41000000UL   /* above original app destination */

/* Staging buffer for loading images before validation */
#define STAGING_ADDR        0x42000000UL
#define STAGING_SIZE        (16U * 1024U * 1024U)

/* Display framebuffers: 1024 x 600 x 2 bytes = 1,228,800 bytes each */
#define FB0_ADDR            0x43000000UL
#define FB1_ADDR            0x43200000UL

/* Original FNIRSI application */
#define ORIG_APP_NOR_OFFSET 0x000A8000UL   /* offset in SPI NOR */
#define ORIG_APP_LOAD_ADDR  0x40000000UL   /* RAM load/entry address */

/* Stack for test payloads */
#define STACK_TOP           0x40020000UL

#endif /* MEMORY_MAP_H */
