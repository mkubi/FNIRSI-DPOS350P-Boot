#include <stdint.h>
#include "../../drivers/fpga_bus_ro.h"
#include "../../drivers/r11_csi.h"
#include "../../drivers/ms5351_stock.h"
#include "../../arch/r11/cache.h"

#define TEST_BASE       0x42000000U
#define GUARD_SIZE      0x00001000U
#define CAPTURE_SIZE    0x00002134U
#define ACTIVE_PAYLOAD_SIZE 0x00001800U
#define CSI_POLL_ITERATIONS 24000000U
#define LEFT_GUARD      ((volatile uint8_t *)TEST_BASE)
#define CAPTURE_BUFFER  ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE))
#define RIGHT_GUARD     ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE + CAPTURE_SIZE))
#define SENTINEL        0xAAU
#define LEFT_SENTINEL   0x5AU
#define RIGHT_SENTINEL  0xA5U

void diag_ui_init(void);
void diag_ui_capture_screen(int fpga_ok, int smec_ok,
                            const struct fpga_smec_init_result *smec,
                            int ready, uint32_t ready_status,
                            uint32_t ready_polls, int clock_ok, int config_ok,
                            uint32_t buffer_reg, uint32_t status_before,
                            int done, uint32_t status_after,
                            uint32_t mode_after, int left_guard_ok,
                            int right_guard_ok, uint32_t changed,
                            uint32_t first_change, uint32_t last_change,
                            uint32_t data0, uint32_t data1,
                            uint32_t data2, uint32_t data3);

static void fill(volatile uint8_t *p, uint32_t count, uint8_t value)
{
    uint32_t i;
    for (i = 0U; i < count; ++i)
        p[i] = value;
}

static int check(volatile const uint8_t *p, uint32_t count, uint8_t value)
{
    uint32_t i;
    for (i = 0U; i < count; ++i)
        if (p[i] != value)
            return 0;
    return 1;
}

static uint32_t load_u32(volatile const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void csi_stage3b_main(void)
{
    struct fpga_diag_result fpga;
    struct ms5351_stock_result clock;
    struct fpga_acq_position_result position;
    struct fpga_smec_init_result smec;
    struct r11_csi_init_result init;
    struct r11_csi_config_result config;
    uint32_t status_before;
    uint32_t status_after;
    uint32_t mode_after;
    uint32_t changed = 0U;
    uint32_t first = 0xFFFFFFFFU;
    uint32_t last = 0xFFFFFFFFU;
    uint32_t i;
    int smec_ok;
    int init_ok;
    int config_ok;
    int done;

    diag_ui_init();
    ms5351_stock_init_95m_10m(&clock);
    /* The stock-decoded selector is known not to reproduce the ID in the
     * standalone startup state. Keep the HW-confirmed baseline selector for
     * the presence check; the capture commands below still use stock primitives. */
    fpga_diag_read_id_baseline(&fpga);
    if (!clock.completed)
        fpga.present = 0;

    /* Stage 21 changes one variable: reproduce the seven HW-confirmed
     * SMEC_Init startup jobs before the otherwise unchanged CSI smoke test. */
    smec_ok = fpga.present ? fpga_smec_init_baseline(&smec) : 0;

    /* Stage 5 HW-confirmed upstream preparation.  Do not touch CSI unless
     * FPGA 0x0A bit 0 reports ready within the stock 2500-poll window. */
    if (smec_ok)
        fpga_acq_position_probe(4U, 2500U, &position);
    else {
        position.ready.ready = 0;
        position.ready.polls = 0U;
        position.ready.final_status = 0U;
        position.ready.mode_byte = 4U;
        position.valid_status = 0U;
        position.position = 0U;
        position.valid = 0;
    }
    if (position.ready.ready && position.valid) {
        /* RAW_BANK_SIZE is 0x2134 bytes.  Stock 0x400405E0 programs 0x22
         * immediately before the per-bank capture loop, using the same
         * big-endian 16-bit command convention. */
        /* Keep the previously reconstructed FPGA-side quantity command for
         * this first standalone run. Success is judged by CSI completion and
         * RAM modification, not by PE_DATA transitions. */
        fpga_set_stream_quantity((uint16_t)CAPTURE_SIZE);
        fpga_prepare_csi_bank0_position(position.position);
    }

    init_ok = (position.ready.ready && position.valid) ?
        r11_csi_prepare_no_capture((uintptr_t)CAPTURE_BUFFER, &init) : 0;
    config_ok = init_ok ? r11_csi_configure_no_capture(&config) : 0;

    fill(LEFT_GUARD, GUARD_SIZE, LEFT_SENTINEL);
    fill(CAPTURE_BUFFER, CAPTURE_SIZE, SENTINEL);
    fill(RIGHT_GUARD, GUARD_SIZE, RIGHT_SENTINEL);
    if (cache_dcache_enabled())
        cache_clean_range(TEST_BASE, GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    status_before = (position.ready.ready && position.valid) ? r11_csi_status() : 0U;
    status_after = 0U;
    done = 0;

    if (position.ready.ready && position.valid && init_ok && config_ok) {
        r11_csi_ack(R11_CSI_DONE);

        /* Exact stock per-bank transition around 0x40040638. */
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
    mode_after = position.ready.ready ? r11_csi_mode() : 0U;

    if (cache_dcache_enabled())
        cache_invalidate_range(TEST_BASE,
                               GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    /* Stock HW changed the first 0x1800 bytes. Scan the full 0x2134 slot. */
    (void)ACTIVE_PAYLOAD_SIZE;
    for (i = 0U; i < CAPTURE_SIZE; ++i) {
        if (CAPTURE_BUFFER[i] != SENTINEL) {
            if (changed == 0U)
                first = i;
            last = i;
            ++changed;
        }
    }

    diag_ui_capture_screen(fpga.present, smec_ok, &smec,
                           position.ready.ready && position.valid,
                           position.position, position.ready.polls,
                           init_ok, config_ok,
                           position.ready.ready ? r11_csi_buffer0() : 0U, status_before,
                           done, status_after, mode_after,
                           check(LEFT_GUARD, GUARD_SIZE, LEFT_SENTINEL),
                           check(RIGHT_GUARD, GUARD_SIZE, RIGHT_SENTINEL),
                           changed, first, last,
                           load_u32(CAPTURE_BUFFER + 0U),
                           load_u32(CAPTURE_BUFFER + 4U),
                           load_u32(CAPTURE_BUFFER + 8U),
                           load_u32(CAPTURE_BUFFER + 12U));

    for (;;)
        __asm__ volatile ("wfi");
}
