#include <stdint.h>
#include "../../drivers/fpga_bus.h"
#include "../../drivers/r11_csi.h"
#include "../../arch/r11/cache.h"

#define TEST_BASE            0x42000000U
#define GUARD_SIZE           0x00001000U
#define CAPTURE_SIZE         0x00002134U
#define CSI_POLL_ITERATIONS  24000000U

#define LEFT_GUARD     ((volatile uint8_t *)TEST_BASE)
#define CAPTURE_BUFFER ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE))
#define RIGHT_GUARD    ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE + CAPTURE_SIZE))

#define SENTINEL       0xAAU
#define LEFT_SENTINEL  0x5AU
#define RIGHT_SENTINEL 0xA5U

void diag_ui_init(void);
void diag_ui_clear(void);
void diag_ui_text_at(unsigned int column, unsigned int row,
                     const char *text);
void diag_ui_hex32_at(unsigned int column, unsigned int row,
                      const char *label, uint32_t value);
void diag_ui_bool_at(unsigned int column, unsigned int row,
                     const char *label, int value);

static void fill_bytes(volatile uint8_t *p, uint32_t count, uint8_t value)
{
    uint32_t i;
    for (i = 0U; i < count; ++i)
        p[i] = value;
}

static int bytes_are(volatile const uint8_t *p,
                     uint32_t count,
                     uint8_t value)
{
    uint32_t i;
    for (i = 0U; i < count; ++i) {
        if (p[i] != value)
            return 0;
    }
    return 1;
}

static uint32_t load_u32(volatile const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void csi_stage27a_main(void)
{
    struct fpga_diag_result fpga;
    struct fpga_acq_position_result position;
    struct r11_csi_stock_parity_result parity;
    uint32_t status_before;
    uint32_t status_after;
    uint32_t changed;
    uint32_t first;
    uint32_t last;
    uint32_t i;
    int parity_ok;
    int ready_ok;
    int done;
    int left_ok;
    int right_ok;

    parity.pll_ready = 0;
    parity.initial_if_cfg = 0U;
    parity.initial_hsize = 0U;
    parity.final_if_cfg = 0U;
    parity.final_hsize = 0U;
    parity.csi_pe_cfg0 = 0U;
    parity.csi_pe_cfg1 = 0U;
    parity.buffer0 = 0U;
    parity.irq_enable = 0U;
    parity.mode = 0U;

    position.ready.ready = 0;
    position.ready.polls = 0U;
    position.ready.final_status = 0U;
    position.ready.mode_byte = 4U;
    position.valid_status = 0U;
    position.position = 0U;
    position.valid = 0;

    diag_ui_init();
    diag_ui_clear();

    fpga_diag_read_id_baseline(&fpga);

    /*
     * Use the confirmed Stage 26 sequence, but point BUF0 at the private
     * Stage 27 guarded destination rather than stock RAM.
     */
    parity_ok = fpga.present ?
        r11_csi_stock_parity_init((uintptr_t)CAPTURE_BUFFER, &parity) : 0;

    /* The acquisition command primitives require the GPIO control bus. */
    fpga_bus_safe_idle();

    if (fpga.present && parity_ok)
        fpga_acq_position_probe(4U, 2500U, &position);

    ready_ok = position.ready.ready && position.valid;

    if (ready_ok) {
        fpga_set_stream_quantity((uint16_t)CAPTURE_SIZE);
        fpga_prepare_csi_bank0_position(position.position);
    }

    fill_bytes(LEFT_GUARD, GUARD_SIZE, LEFT_SENTINEL);
    fill_bytes(CAPTURE_BUFFER, CAPTURE_SIZE, SENTINEL);
    fill_bytes(RIGHT_GUARD, GUARD_SIZE, RIGHT_SENTINEL);

    if (cache_dcache_enabled())
        cache_clean_range(TEST_BASE, GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    status_before = 0U;
    status_after = 0U;
    done = 0;

    if (fpga.present && parity_ok && ready_ok) {
        /* Re-enter the HW-confirmed sample-plane state after FPGA commands. */
        fpga_bus_enter_csi_mode();
        r11_csi_set_buffer0((uintptr_t)CAPTURE_BUFFER);
        r11_csi_ack(R11_CSI_DONE);

        status_before = r11_csi_status();

        r11_csi_timer0_gate(0);
        r11_csi_capture_enable();
        fpga_capture_gate(1);

        done = r11_csi_wait_done_bounded(CSI_POLL_ITERATIONS,
                                         &status_after);

        fpga_capture_gate(0);
        r11_csi_timer0_gate(1);
        r11_csi_ack(R11_CSI_DONE);
        r11_csi_capture_disable();
        fpga_bus_leave_csi_mode();
    }

    if (cache_dcache_enabled())
        cache_invalidate_range(TEST_BASE,
                               GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    left_ok = bytes_are(LEFT_GUARD, GUARD_SIZE, LEFT_SENTINEL);
    right_ok = bytes_are(RIGHT_GUARD, GUARD_SIZE, RIGHT_SENTINEL);

    changed = 0U;
    first = 0xFFFFFFFFU;
    last = 0xFFFFFFFFU;
    for (i = 0U; i < CAPTURE_SIZE; ++i) {
        if (CAPTURE_BUFFER[i] != SENTINEL) {
            if (changed == 0U)
                first = i;
            last = i;
            ++changed;
        }
    }

    diag_ui_text_at(2U, 1U, "CUSTOM CSI CAPTURE - STAGE 27A");
    diag_ui_text_at(2U, 3U, "CONTROL / CSI");
    diag_ui_text_at(66U, 3U, "BUFFER");

    diag_ui_bool_at(2U, 5U, "FPGA ID:       ", fpga.present);
    diag_ui_bool_at(2U, 6U, "CSI PARITY:    ", parity_ok);
    diag_ui_bool_at(2U, 7U, "SMEC USED:     ", 0);
    diag_ui_bool_at(2U, 9U, "0A READY:      ", position.ready.ready);
    diag_ui_bool_at(2U, 10U, "18 VALID:      ", position.valid);
    diag_ui_hex32_at(2U, 11U, "14 POSITION:   ", position.position);
    diag_ui_hex32_at(2U, 12U, "0A POLLS:      ", position.ready.polls);
    diag_ui_hex32_at(2U, 14U, "BUF0:          ", r11_csi_buffer0());
    diag_ui_hex32_at(2U, 15U, "STATUS PRE:    ", status_before);
    diag_ui_bool_at(2U, 16U, "CSI DONE:      ", done);
    diag_ui_hex32_at(2U, 17U, "STATUS POST:   ", status_after);

    diag_ui_bool_at(66U, 5U, "LEFT GUARD:   ", left_ok);
    diag_ui_bool_at(66U, 6U, "RIGHT GUARD:  ", right_ok);
    diag_ui_hex32_at(66U, 8U, "CHANGED:      ", changed);
    diag_ui_hex32_at(66U, 9U, "FIRST:        ", first);
    diag_ui_hex32_at(66U, 10U, "LAST:         ", last);
    diag_ui_hex32_at(66U, 12U, "DATA 00:      ", load_u32(CAPTURE_BUFFER + 0U));
    diag_ui_hex32_at(66U, 13U, "DATA 04:      ", load_u32(CAPTURE_BUFFER + 4U));
    diag_ui_hex32_at(66U, 14U, "DATA 08:      ", load_u32(CAPTURE_BUFFER + 8U));
    diag_ui_hex32_at(66U, 15U, "DATA 0C:      ", load_u32(CAPTURE_BUFFER + 12U));

    diag_ui_bool_at(2U, 20U, "CAPTURE PASS:  ",
                    fpga.present && parity_ok && ready_ok && done &&
                    left_ok && right_ok && changed != 0U);
    diag_ui_text_at(2U, 23U, "NO SMEC - BOUNDED SINGLE BANK - POWER CYCLE TO EXIT");

    for (;;)
        __asm__ volatile ("wfi");
}
