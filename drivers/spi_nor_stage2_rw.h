#ifndef SPI_NOR_STAGE2_RW_H
#define SPI_NOR_STAGE2_RW_H

#include <stdint.h>

#define SPI_NOR_STAGE2_SECTOR_SIZE 0x1000U

typedef struct {
    int init_result;
    int status_before_result;
    uint8_t status_before;
    int write_enable_result;
    int status_after_enable_result;
    uint8_t status_after_enable;
    int write_disable_result;
    int status_after_disable_result;
    uint8_t status_after_disable;
} spi_nor_stage2_wel_diag_t;

/* Non-destructive: reads status, sets WEL, clears WEL, and reads status. */
int spi_nor_stage2_wel_diagnostic(spi_nor_stage2_wel_diag_t *diag);

int spi_nor_stage2_rw_init(void);

typedef struct {
    int init_result;
    int status_before_result;
    uint8_t status_before;
    int write_enable_result;
    int erase_tx_result;
    int status_after_command_result;
    uint8_t status_after_command;
    uint32_t poll_count;
    uint32_t elapsed_ms;
    uint8_t busy_seen;
    uint8_t final_status;
    int wait_result;
} spi_nor_stage2_erase_diag_t;

/* Detailed erase path for a pre-verified erased scratch sector only. */
int spi_nor_stage2_erase_diagnostic(uint32_t address,
                                    spi_nor_stage2_erase_diag_t *diag);

int spi_nor_stage2_erase_sector(uint32_t address);
int spi_nor_stage2_program(uint32_t address, const uint8_t *data,
                           uint32_t length);

#endif
