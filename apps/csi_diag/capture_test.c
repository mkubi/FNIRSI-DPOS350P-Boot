#include <stdint.h>
#include "../../drivers/fpga_bus_ro.h"
#include "../../drivers/r11_csi.h"
#include "../../arch/r11/cache.h"

#define TEST_BASE       0x42000000U
#define GUARD_SIZE      0x00001000U
#define CAPTURE_SIZE    0x00100000U
#define LEFT_GUARD      ((volatile uint8_t *)TEST_BASE)
#define CAPTURE_BUFFER  ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE))
#define RIGHT_GUARD     ((volatile uint8_t *)(TEST_BASE + GUARD_SIZE + CAPTURE_SIZE))
#define SENTINEL        0xAAU
#define LEFT_SENTINEL   0x5AU
#define RIGHT_SENTINEL  0xA5U

void diag_ui_init(void);
void diag_ui_capture_screen(int fpga_ok, int clock_ok, int config_ok,
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
    struct fpga_capture_arm_probe_result arm;
    struct r11_csi_init_result init;
    struct r11_csi_config_result config;
    uint32_t status_before;
    uint32_t status_after;
    uint32_t mode_after;
    uint32_t changed = 0U;
    uint32_t first = 0xFFFFFFFFU;
    uint32_t last = 0xFFFFFFFFU;
    uint32_t i;
    int init_ok;
    int config_ok;
    int done;

    diag_ui_init();
    fpga_diag_read_id(&fpga);
    fpga_select_bank0();
    fpga_capture_arm_probe(&arm);

    init_ok = r11_csi_prepare_no_capture((uintptr_t)CAPTURE_BUFFER, &init);
    config_ok = init_ok ? r11_csi_configure_no_capture(&config) : 0;

    fill(LEFT_GUARD, GUARD_SIZE, LEFT_SENTINEL);
    fill(CAPTURE_BUFFER, CAPTURE_SIZE, SENTINEL);
    fill(RIGHT_GUARD, GUARD_SIZE, RIGHT_SENTINEL);
    if (cache_dcache_enabled())
        cache_clean_range(TEST_BASE, GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    status_before = r11_csi_status();
    r11_csi_ack(0xFFU);
    r11_csi_capture_enable();
    done = r11_csi_wait_done(100U, &status_after);
    r11_csi_capture_disable();
    mode_after = r11_csi_mode();

    if (cache_dcache_enabled())
        cache_invalidate_range(TEST_BASE,
                               GUARD_SIZE + CAPTURE_SIZE + GUARD_SIZE);

    for (i = 0U; i < CAPTURE_SIZE; ++i) {
        if (CAPTURE_BUFFER[i] != SENTINEL) {
            if (changed == 0U)
                first = i;
            last = i;
            ++changed;
        }
    }

    diag_ui_capture_screen(fpga.present, init_ok, config_ok,
                           r11_csi_buffer0(), status_before,
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
