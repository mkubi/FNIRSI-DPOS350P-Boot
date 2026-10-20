/*
 * FNIRSI DPOS350P boot manager — Phase 0 skeleton
 *
 * Phase 0: FEL workflow
 *   xfel ddr v3s
 *   xfel write 0x41000000 dpos-boot.bin
 *   xfel exec  0x41000000
 *
 * Initializes display and touch, then reads first 64 bytes from
 * SPI NOR and sector 0 from SD-NAND, showing pass/fail on screen.
 */
#include "drivers/display.h"
#include "drivers/gt911.h"

void main(void)
{
    display_init();
    display_fill_back(COLOR_BLACK);
    display_swap();

    gt911_init();

    /* TODO Phase 1: SPI NOR read test */
    /* TODO Phase 2: SDC2 raw sector read test */
    /* TODO Phase 4: boot menu */

    for (;;) {
        /* placeholder loop */
    }
}
