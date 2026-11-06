#include <stdint.h>

#include "fel_recovery.h"
#include "spi_nor_ro.h"

static void halt_forever(void)
{
    for (;;)
        __asm__ volatile ("wfi");
}

int main(void)
{
    spi_nor_jedec_id_t jedec;

    if (spi_nor_ro_init() != 0)
        halt_forever();

    if (spi_nor_read_jedec_id(&jedec) != 0)
        halt_forever();

    /*
     * Never enter recovery if the expected boot NOR was not positively
     * identified. This keeps the test fail-safe.
     */
    if (jedec.manufacturer != 0xEFU ||
        jedec.memory_type != 0x40U ||
        jedec.capacity != 0x15U)
        halt_forever();

    stage1_enter_fel_recovery();
}
