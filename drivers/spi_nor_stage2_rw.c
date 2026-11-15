#include <stdint.h>
#include "spi0.h"
#include "spi_nor_stage2_rw.h"
#include "timer.h"

#define CMD_WRITE_ENABLE  0x06U
#define CMD_WRITE_DISABLE 0x04U
#define CMD_READ_STATUS1  0x05U
#define CMD_SECTOR_ERASE  0x20U
#define CMD_PAGE_PROGRAM  0x02U
#define STATUS_BUSY       0x01U
#define STATUS_WEL        0x02U
#define MAX_PROGRAM_DATA  60U
#define PAGE_SIZE         256U
#define ERASE_READY_TIMEOUT_MS 30000U
#define ERASE_POLL_INTERVAL_MS 1U

static int read_status(uint8_t *status)
{
    const uint8_t tx[2] = { CMD_READ_STATUS1, 0xFFU };
    uint8_t rx[2];
    int result = spi0_transfer(tx, rx, sizeof(tx));
    if (result)
        return result;
    *status = rx[1];
    return 0;
}

static int wait_ready(void)
{
    uint32_t start_ms;
    uint32_t next_poll;
    uint32_t now;
    uint8_t status;
    int result;

    start_ms = timer_get_ms();
    next_poll = start_ms;

    for (;;) {
        now = timer_get_ms();
        if ((uint32_t)(now - start_ms) >= ERASE_READY_TIMEOUT_MS)
            return -20;
        if ((int32_t)(now - next_poll) < 0)
            continue;
        next_poll = now + ERASE_POLL_INTERVAL_MS;

        result = read_status(&status);
        if (result)
            return result;
        if ((status & STATUS_BUSY) == 0U)
            return 0;
    }
}

static int write_enable(void)
{
    const uint8_t command = CMD_WRITE_ENABLE;
    uint8_t status;
    int result;
    result = spi0_transfer(&command, 0, 1U);
    if (result)
        return result;
    result = read_status(&status);
    if (result)
        return result;
    return (status & STATUS_WEL) != 0U ? 0 : -21;
}

int spi_nor_stage2_rw_init(void)
{
    int result = spi0_init();
    if (result)
        return result;
    return wait_ready();
}


int spi_nor_stage2_wel_diagnostic(spi_nor_stage2_wel_diag_t *diag)
{
    const uint8_t write_disable = CMD_WRITE_DISABLE;
    int rc;

    if (!diag)
        return -24;

    diag->init_result = -127;
    diag->status_before_result = -127;
    diag->status_before = 0U;
    diag->write_enable_result = -127;
    diag->status_after_enable_result = -127;
    diag->status_after_enable = 0U;
    diag->write_disable_result = -127;
    diag->status_after_disable_result = -127;
    diag->status_after_disable = 0U;

    rc = spi_nor_stage2_rw_init();
    diag->init_result = rc;
    if (rc)
        return rc;

    rc = read_status(&diag->status_before);
    diag->status_before_result = rc;
    if (rc)
        return rc;

    rc = write_enable();
    diag->write_enable_result = rc;

    /* Read status even when write_enable reports a missing WEL bit. */
    diag->status_after_enable_result =
        read_status(&diag->status_after_enable);
    if (rc)
        goto disable;
    if (diag->status_after_enable_result)
        goto disable;

disable:
    diag->write_disable_result =
        spi0_transfer(&write_disable, 0, 1U);
    diag->status_after_disable_result =
        read_status(&diag->status_after_disable);

    if (rc)
        return rc;
    if (diag->status_after_enable_result)
        return diag->status_after_enable_result;
    if (diag->write_disable_result)
        return diag->write_disable_result;
    if (diag->status_after_disable_result)
        return diag->status_after_disable_result;
    if ((diag->status_after_enable & STATUS_WEL) == 0U)
        return -21;
    if ((diag->status_after_disable & STATUS_WEL) != 0U)
        return -25;
    return 0;
}

int spi_nor_stage2_erase_diagnostic(uint32_t address,
                                    spi_nor_stage2_erase_diag_t *diag)
{
    uint8_t tx[4];
    uint8_t status = 0U;
    uint32_t start_ms;
    uint32_t next_poll;
    uint32_t now;
    int rc;

    if (!diag)
        return -24;
    if ((address & (SPI_NOR_STAGE2_SECTOR_SIZE - 1U)) != 0U)
        return -22;

    diag->init_result = -127;
    diag->status_before_result = -127;
    diag->status_before = 0U;
    diag->write_enable_result = -127;
    diag->erase_tx_result = -127;
    diag->status_after_command_result = -127;
    diag->status_after_command = 0U;
    diag->poll_count = 0U;
    diag->elapsed_ms = 0U;
    diag->busy_seen = 0U;
    diag->final_status = 0U;
    diag->wait_result = -127;

    rc = spi_nor_stage2_rw_init();
    diag->init_result = rc;
    if (rc)
        return rc;
    rc = read_status(&diag->status_before);
    diag->status_before_result = rc;
    if (rc)
        return rc;
    rc = write_enable();
    diag->write_enable_result = rc;
    if (rc)
        return rc;

    tx[0] = CMD_SECTOR_ERASE;
    tx[1] = (uint8_t)(address >> 16);
    tx[2] = (uint8_t)(address >> 8);
    tx[3] = (uint8_t)address;
    rc = spi0_transfer(tx, 0, sizeof(tx));
    diag->erase_tx_result = rc;
    if (rc)
        return rc;

    start_ms = timer_get_ms();
    next_poll = start_ms;

    rc = read_status(&status);
    diag->status_after_command_result = rc;
    diag->status_after_command = status;
    if (rc) {
        diag->wait_result = rc;
        return rc;
    }
    if ((status & STATUS_BUSY) != 0U)
        diag->busy_seen = 1U;

    for (;;) {
        now = timer_get_ms();
        diag->elapsed_ms = now - start_ms;

        if ((status & STATUS_BUSY) == 0U) {
            diag->final_status = status;
            diag->wait_result = 0;
            return 0;
        }
        if (diag->elapsed_ms >= ERASE_READY_TIMEOUT_MS) {
            diag->final_status = status;
            diag->wait_result = -20;
            return -20;
        }
        if ((int32_t)(now - next_poll) < 0)
            continue;
        next_poll = now + ERASE_POLL_INTERVAL_MS;

        rc = read_status(&status);
        ++diag->poll_count;
        if (rc) {
            diag->final_status = status;
            diag->wait_result = rc;
            return rc;
        }
        if ((status & STATUS_BUSY) != 0U)
            diag->busy_seen = 1U;
    }
}


int spi_nor_stage2_erase_sector(uint32_t address)
{
    uint8_t tx[4];
    int result;
    if ((address & (SPI_NOR_STAGE2_SECTOR_SIZE - 1U)) != 0U)
        return -22;
    result = write_enable();
    if (result)
        return result;
    tx[0] = CMD_SECTOR_ERASE;
    tx[1] = (uint8_t)(address >> 16);
    tx[2] = (uint8_t)(address >> 8);
    tx[3] = (uint8_t)address;
    result = spi0_transfer(tx, 0, sizeof(tx));
    if (result)
        return result;
    return wait_ready();
}

int spi_nor_stage2_program(uint32_t address, const uint8_t *data,
                           uint32_t length)
{
    uint8_t tx[64];
    uint32_t chunk;
    uint32_t page_remaining;
    uint32_t i;
    int result;
    if (!data || length == 0U)
        return -23;
    while (length != 0U) {
        page_remaining = PAGE_SIZE - (address & (PAGE_SIZE - 1U));
        chunk = length;
        if (chunk > MAX_PROGRAM_DATA)
            chunk = MAX_PROGRAM_DATA;
        if (chunk > page_remaining)
            chunk = page_remaining;
        result = write_enable();
        if (result)
            return result;
        tx[0] = CMD_PAGE_PROGRAM;
        tx[1] = (uint8_t)(address >> 16);
        tx[2] = (uint8_t)(address >> 8);
        tx[3] = (uint8_t)address;
        for (i = 0U; i < chunk; ++i)
            tx[4U + i] = data[i];
        result = spi0_transfer(tx, 0, chunk + 4U);
        if (result)
            return result;
        result = wait_ready();
        if (result)
            return result;
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return 0;
}
