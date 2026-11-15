#ifndef STAGE2_UPDATE_H
#define STAGE2_UPDATE_H

#include <stdint.h>
#include "spi_nor_stage2_rw.h"

typedef struct {
    uint32_t file_size;
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t payload_size;
    uint32_t load_address;
    uint32_t entry_address;
    uint32_t expected_crc32;
    uint32_t calculated_crc32;
    uint32_t installed_expected_crc32;
    uint32_t installed_calculated_crc32;
    uint32_t differences;
    uint32_t first_difference;
    int sd_result;
    int fat_result;
    int file_result;
    int input_read_result;
    int nor_result;
    int file_found;
    int header_valid;
    int input_valid;
    int slot_valid;
    int exact_match;
} stage2_update_info_t;

typedef enum {
    STAGE2_WRITE_OK = 0,
    STAGE2_WRITE_NOT_ARMED,
    STAGE2_WRITE_INPUT_INVALID,
    STAGE2_WRITE_BACKUP_FAILED,
    STAGE2_WRITE_INIT_FAILED,
    STAGE2_WRITE_ERASE_FAILED,
    STAGE2_WRITE_PROGRAM_FAILED,
    STAGE2_WRITE_VERIFY_FAILED,
    STAGE2_WRITE_RESTORED,
    STAGE2_WRITE_RESTORE_FAILED
} stage2_write_status_t;

#define STAGE2_STEP_NOT_RUN (-127)

typedef struct {
    stage2_write_status_t status;
    uint32_t first_difference;
    int confirm_found;
    int confirm_valid;
    int backup_result;
    int write_init_result;
    int erase_result;
    int program_result;
    int verify_result;
    int restore_result;
} stage2_write_result_t;

#define STAGE2_TAIL_FIRST_ADDRESS 0x001EB000U
#define STAGE2_TAIL_SECTOR_COUNT  21U

typedef struct {
    uint32_t address;
    uint32_t non_ff_count;
    uint32_t first_non_ff;
    uint32_t first_word;
    int read_result;
} stage2_tail_sector_t;

int stage2_tail_scan(stage2_tail_sector_t *sectors, unsigned capacity);
int stage2_tail_verify_erased(uint32_t address,
                              uint32_t *non_ff_count,
                              uint32_t *first_non_ff);


int stage2_update_analyze(stage2_update_info_t *info);

/*
 * Confirm-gated writer. Requires /STG2OK.BIN with the exact 16-byte token.
 * No reboot is performed. On write/verify failure, the old slot is restored.
 */
stage2_write_status_t stage2_update_write(stage2_write_result_t *result);

#endif /* STAGE2_UPDATE_H */
