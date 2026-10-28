#include <stdint.h>
#include "../../drivers/fpga_bus.h"

/* Adapter functions supplied by ui_adapter.c. */
void diag_ui_init(void);
void diag_ui_clear(void);
void diag_ui_puts(const char *s);
void diag_ui_hex32(const char *label, uint32_t value);
void diag_ui_hex8x5(const char *label, const uint8_t value[5]);

void main(void)
{
    struct fpga_diag_result result;

    diag_ui_init();
    diag_ui_clear();
    diag_ui_puts("DPOS350P FPGA DIAGNOSTIC\n");
    diag_ui_puts("MINIMAL ID HANDSHAKE\n");
    diag_ui_puts("NO ANALOG INIT\n\n");

    fpga_diag_read_id(&result);

    diag_ui_hex32("PE CFG0 BEFORE: ", result.pe_cfg0_before);
    diag_ui_hex32("PE CFG1 BEFORE: ", result.pe_cfg1_before);
    diag_ui_hex32("PE DATA BEFORE: ", result.pe_data_before);
    diag_ui_puts("\n");
    diag_ui_hex8x5("RAW5:          ", result.raw);
    diag_ui_hex32("ID [0..3]:      ", result.id_word);
    diag_ui_hex32("ID [1..4]:      ", result.id_word_shifted);
    
    if (result.present)
        diag_ui_puts("FPGA PRESENT:    YES\n");
    else
        diag_ui_puts("FPGA PRESENT:    NO\n");

    diag_ui_puts("\nOBSERVE RELAY CLICK\n");
    diag_ui_puts("POWER CYCLE TO EXIT\n");

    for (;;)
        __asm__ volatile ("wfi");
}
