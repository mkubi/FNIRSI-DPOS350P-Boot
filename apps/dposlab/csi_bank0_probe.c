#include <stdint.h>

#include "csi_bank0_probe.h"
#include "../../arch/r11/cache.h"
#include "../../drivers/fpga_bus_ro.h"
#include "../../drivers/r11_csi.h"

#define STOCK_PREPARE_CYCLIC_POSITION ((void (*)(void))0x4003FAB8U)

#define TEST_BASE            0x42000000U
#define GUARD_SIZE           0x00001000U
#define CAPTURE_SIZE         0x00002134U
#define BLOCK_SIZE           0x00000100U
#define BLOCK_COUNT          ((CAPTURE_SIZE + BLOCK_SIZE - 1U) / BLOCK_SIZE)
#define CSI_POLL_ITERATIONS  24000000U

#define LEFT_GUARD     ((volatile uint8_t *)TEST_BASE)
#define CAPTURE_BUFFER ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE))
#define RIGHT_GUARD    ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE + CAPTURE_SIZE))

#define SENTINEL       0xAAU
#define LEFT_SENTINEL  0x5AU
#define RIGHT_SENTINEL 0xA5U

struct dposlab_csi_bank0_result dposlab_csi_bank0_result
    __attribute__((section(".data")));

static void fill_bytes(volatile uint8_t *p, uint32_t count, uint8_t value)
{
    uint32_t i;
    for (i = 0U; i < count; ++i)
        p[i] = value;
}

static uint32_t bytes_are(volatile const uint8_t *p,
                          uint32_t count, uint8_t value)
{
    uint32_t i;
    for (i = 0U; i < count; ++i) {
        if (p[i] != value)
            return 0U;
    }
    return 1U;
}

static uint32_t fnv1a(volatile const uint8_t *p, uint32_t count)
{
    uint32_t i;
    uint32_t hash = 2166136261U;
    for (i = 0U; i < count; ++i)
        hash = (hash ^ p[i]) * 16777619U;
    return hash;
}

static uint32_t load_u32(volatile const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void dposlab_csi_bank0_capture(uint8_t mode_byte, int ready_before)
{
    struct dposlab_csi_bank0_result *out = &dposlab_csi_bank0_result;
    struct fpga_acq_position_result position;
    uint32_t block_changed[BLOCK_COUNT];
    uint32_t i;

    out->ready_before = ready_before ? 1U : 0U;
    out->attempted = 0U;
    out->done = 0U;
    out->position_valid = 0U;
    out->position = 0U;
    out->position_polls = 0U;
    out->buffer_reg = 0U;
    out->status_before = 0U;
    out->status_after = 0U;
    out->mode_active = 0U;
    out->mode_after = 0U;
    out->changed_bytes = 0U;
    out->changed_blocks = 0U;
    out->first_change = 0xFFFFFFFFU;
    out->last_change = 0xFFFFFFFFU;

    for (i = 0U; i < BLOCK_COUNT; ++i)
        block_changed[i] = 0U;

    fill_bytes(LEFT_GUARD, GUARD_SIZE, LEFT_SENTINEL);
    fill_bytes(CAPTURE_BUFFER, CAPTURE_SIZE, SENTINEL);
    fill_bytes(RIGHT_GUARD, GUARD_SIZE, RIGHT_SENTINEL);
    out->hash_before = fnv1a(CAPTURE_BUFFER, CAPTURE_SIZE);

    if (cache_dcache_enabled())
        cache_clean_range(TEST_BASE, GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    if (!ready_before)
        goto inspect;

    /* Use the exact stock requested-position producer. 0x18/0x14 is read
     * only after capture as a diagnostic actual-position result. */
    position.ready.ready = 1;
    position.ready.polls = 0U;
    position.ready.final_status = 0U;
    position.ready.mode_byte = mode_byte;
    position.valid_status = 0U;
    position.position = 0U;
    position.valid = 0;

    out->attempted = 1U;

    fpga_set_stream_quantity((uint16_t)CAPTURE_SIZE);
    STOCK_PREPARE_CYCLIC_POSITION();
    fpga_select_csi_bank0();

    /* GPIO commands above leave the control bus active. Enter sample mode. */
    fpga_bus_enter_csi_mode();
    r11_csi_set_buffer0((uintptr_t)CAPTURE_BUFFER);
    r11_csi_ack(R11_CSI_DONE);
    out->buffer_reg = r11_csi_buffer0();
    out->status_before = r11_csi_status();

    /* Exact bounded stock per-bank capture transition. */
    r11_csi_timer0_gate(0);
    r11_csi_capture_enable();
    out->mode_active = r11_csi_mode();
    fpga_capture_gate(1);
    out->done = r11_csi_wait_done_bounded(CSI_POLL_ITERATIONS,
                                           &out->status_after) ? 1U : 0U;
    fpga_capture_gate(0);
    r11_csi_timer0_gate(1);
    r11_csi_ack(R11_CSI_DONE);
    r11_csi_capture_disable();
    fpga_bus_leave_csi_mode();
    out->mode_after = r11_csi_mode();

    /* Post-capture diagnostic only. It does not gate or alter the capture. */
    fpga_acq_position_read_after_ready(&position);
    out->position_valid = position.valid ? 1U : 0U;
    out->position = position.position;
    out->position_polls = 0U;

inspect:
    if (cache_dcache_enabled())
        cache_invalidate_range(TEST_BASE,
                               GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    out->hash_after = fnv1a(CAPTURE_BUFFER, CAPTURE_SIZE);
    for (i = 0U; i < CAPTURE_SIZE; ++i) {
        if (CAPTURE_BUFFER[i] != SENTINEL) {
            if (out->changed_bytes == 0U)
                out->first_change = i;
            out->last_change = i;
            ++out->changed_bytes;
            block_changed[i / BLOCK_SIZE] = 1U;
        }
    }
    for (i = 0U; i < BLOCK_COUNT; ++i)
        out->changed_blocks += block_changed[i];

    out->left_guard_ok = bytes_are(LEFT_GUARD, GUARD_SIZE, LEFT_SENTINEL);
    out->right_guard_ok = bytes_are(RIGHT_GUARD, GUARD_SIZE, RIGHT_SENTINEL);
    out->data0 = load_u32(CAPTURE_BUFFER + 0U);
    out->data1 = load_u32(CAPTURE_BUFFER + 4U);
    out->data2 = load_u32(CAPTURE_BUFFER + 8U);
    out->data3 = load_u32(CAPTURE_BUFFER + 12U);
}
