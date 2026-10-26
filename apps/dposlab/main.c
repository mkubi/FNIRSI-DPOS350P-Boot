#include <stdint.h>
#include "stock_api.h"
#include "runtime_patch.h"
#include "runtime_snapshot.h"
#include "runtime_trace.h"
#include "dposlab_abi.h"
#include "../../drivers/display.h"
#include "../../ui/console.h"
#include "../../drivers/fpga_bus_ro.h"

#ifndef DPOSLAB_MODE
#define DPOSLAB_MODE 0
#endif
#define WHITE 0xFFFFU
#define BLACK 0x0000U
#define DPOSLAB_PHOTO_DELAY_LOOPS 500000000U

static volatile uint32_t marker __attribute__((section(".data"))) = 0x4C414201U;

static void put_hex(uint32_t v)
{
    static const char h[]="0123456789ABCDEF"; char s[11]; unsigned i;
    s[0]='0'; s[1]='x'; for(i=0;i<8;i++) s[2+i]=h[(v>>(28-i*4))&15]; s[10]=0;
    console_puts(s);
}
#if DPOSLAB_MODE != 0
static void line(const char *n,uint32_t v){ console_puts(n); put_hex(v); console_putln(""); }
#endif
volatile uint32_t dposlab_fpga_hook_hits
    __attribute__((section(".data"))) = 0U;

volatile uint32_t dposlab_fpga_post_cpsr
    __attribute__((section(".data"))) = 0U;

static void photo_delay(void)
{
    volatile uint32_t delay;

    for (delay = 0U; delay < DPOSLAB_PHOTO_DELAY_LOOPS; ++delay)
        __asm__ volatile ("nop");
}

static void put_before_after(const char *name,
                             uint32_t before,
                             uint32_t after)
{
    console_puts(name);
    put_hex(before);
    console_puts(" / ");
    put_hex(after);
    console_putln("");
}

void dposlab_fpga_return_monitor(void)
{
    dposlab_fpga_hook_hits += 1U;
    dposlab_fpga_post_cpsr = dposlab_fpga_after.cpsr;
    marker = 0x4C414246U; /* "LABF" */

    /*
     * Page 1: CPU and PE bus state.
     * All displayed hardware values were captured passively.
     */
    console_init(WHITE, BLACK, 2U);
    console_clear();

    console_putln("DPOSLAB FPGA DIFF 1/2");
    console_putln("---------------------------");
    console_putln("FPGA_Start RETURNED        PASS");

    console_puts("HOOK COUNT                 ");
    put_hex(dposlab_fpga_hook_hits);
    console_putln("");

    put_before_after("CPSR B/A   ",
                     dposlab_fpga_before.cpsr,
                     dposlab_fpga_after.cpsr);

    console_puts("GPR CHANGED MASK           ");
    put_hex(dposlab_gpr_changed_mask());
    console_putln("");

    put_before_after("PE CFG0 B/A ",
                     dposlab_fpga_before.pe_cfg0,
                     dposlab_fpga_after.pe_cfg0);

    put_before_after("PE CFG1 B/A ",
                     dposlab_fpga_before.pe_cfg1,
                     dposlab_fpga_after.pe_cfg1);

    put_before_after("PE DATA B/A ",
                     dposlab_fpga_before.pe_data,
                     dposlab_fpga_after.pe_data);

    put_before_after("PE PULL B/A ",
                     dposlab_fpga_before.pe_pull0,
                     dposlab_fpga_after.pe_pull0);

    console_puts("STOCK SP                   ");
    put_hex(dposlab_fpga_before.stock_sp);
    console_putln("");

    console_puts("FPGA EXEC SP               ");
    put_hex(dposlab_fpga_before.exec_sp);
    console_putln("");

    console_putln("");
    console_putln("AUTO NEXT PAGE             ~3 SEC");
    console_present();
    photo_delay();

    /*
     * Page 2: passive clock and CSI state.
     */
    console_clear();

    console_putln("DPOSLAB FPGA DIFF 2/2");
    console_putln("---------------------------");

    put_before_after("CCU GATE0   ",
                     dposlab_fpga_before.ccu_gate0,
                     dposlab_fpga_after.ccu_gate0);

    put_before_after("CCU RESET0  ",
                     dposlab_fpga_before.ccu_reset0,
                     dposlab_fpga_after.ccu_reset0);

    put_before_after("CSI IF CFG  ",
                     dposlab_fpga_before.csi_if_cfg,
                     dposlab_fpga_after.csi_if_cfg);

    put_before_after("CSI MODE    ",
                     dposlab_fpga_before.csi_mode,
                     dposlab_fpga_after.csi_mode);

    put_before_after("CSI HSIZE   ",
                     dposlab_fpga_before.csi_hsize,
                     dposlab_fpga_after.csi_hsize);

    put_before_after("CSI DEST    ",
                     dposlab_fpga_before.csi_destination,
                     dposlab_fpga_after.csi_destination);

    put_before_after("CSI IRQ EN  ",
                     dposlab_fpga_before.csi_irq_enable,
                     dposlab_fpga_after.csi_irq_enable);

    put_before_after("CSI STATUS  ",
                     dposlab_fpga_before.csi_status,
                     dposlab_fpga_after.csi_status);

    console_putln("");

    console_putln("PASSIVE MMIO SNAPSHOT      OK");
    console_putln("AUTO CONTINUE              ~3 SEC");
    console_present();
    photo_delay();
    {
        const volatile struct dposlab_trace_record *first;
        const volatile struct dposlab_trace_record *last;

        first = dposlab_trace_first();
        last = dposlab_trace_last();

        console_clear();

        console_putln("DPOSLAB TRACE STATUS 3/3");
        console_putln("---------------------------");

        console_puts("TRACE MAGIC                ");
        put_hex(dposlab_trace.magic);
        console_putln("");

        console_puts("TRACE VERSION              ");
        put_hex(dposlab_trace.version);
        console_putln("");

        console_puts("HEADER SIZE                ");
        put_hex(dposlab_trace.header_size);
        console_putln("");

        console_puts("RECORD SIZE                ");
        put_hex(dposlab_trace.record_size);
        console_putln("");

        console_puts("TRACE CAPACITY             ");
        put_hex(dposlab_trace.capacity);
        console_putln("");

        console_puts("TRACE RECORDS              ");
        put_hex(dposlab_trace.record_count);
        console_putln("");

        console_puts("TRACE DROPPED              ");
        put_hex(dposlab_trace.dropped);
        console_putln("");

        console_puts("FIRST CHECKPOINT           ");
        put_hex(first ? first->checkpoint : 0xFFFFFFFFU);
        console_putln("");

        console_puts("FIRST ADDRESS              ");
        put_hex(first ? first->execution_address : 0U);
        console_putln("");

        console_puts("LAST CHECKPOINT            ");
        put_hex(last ? last->checkpoint : 0xFFFFFFFFU);
        console_putln("");

        console_puts("LAST ADDRESS               ");
        put_hex(last ? last->execution_address : 0U);
        console_putln("");

        console_putln("");
        console_putln("AUTO CONTINUE              ~3 SEC");
        console_present();
        photo_delay();
    }

}

int main(const dposlab_handoff_t *handoff)
{
    display_init(); console_init(WHITE,BLACK,2U); console_clear();
    console_putln("DPOSLAB STAGE 3 V1");
    console_putln("STEP                         RESULT / VALUE");
    console_putln("-------------------------------------------");
    console_puts("HANDOFF PTR                  ");
    put_hex((uint32_t)(uintptr_t)handoff);
    console_putln("");
    console_present();

    if (!handoff) {
        console_putln("HANDOFF POINTER              NULL");
        console_putln("HANDOFF ABI                  FAIL");
        console_present();
        for (;;)
            __asm__ volatile ("wfi");
    }

    console_puts("MAGIC                        ");
    put_hex(handoff->magic);
    console_putln("");

    console_puts("VERSION                      ");
    put_hex(handoff->version);
    console_putln("");

    console_puts("SIZE                         ");
    put_hex(handoff->size);
    console_putln("");

    console_puts("EXPECTED SIZE                ");
    put_hex((uint32_t)sizeof(*handoff));
    console_putln("");

    console_puts("FLAGS                        ");
    put_hex(handoff->flags);
    console_putln("");

    console_puts("STAGE2 SP                    ");
    put_hex(handoff->stage2_sp);
    console_putln("");

    console_puts("EXE2 BASE                    ");
    put_hex(handoff->exe2_base);
    console_putln("");

    console_puts("EXE2 SIZE                    ");
    put_hex(handoff->exe2_size);
    console_putln("");

    console_puts("EXE2 ENTRY                   ");
    put_hex(handoff->exe2_entry);
    console_putln("");

    console_puts("DPOSLAB BASE                 ");
    put_hex(handoff->dposlab_base);
    console_putln("");

    console_puts("DPOSLAB SIZE                 ");
    put_hex(handoff->dposlab_size);
    console_putln("");

    console_puts("DPOSLAB ENTRY                ");
    put_hex(handoff->dposlab_entry);
    console_putln("");

    console_present();

    if (handoff->magic != DPOSLAB_HANDOFF_MAGIC) {
        console_putln("HANDOFF MAGIC                FAIL");
        console_present();
        for (;;)
            __asm__ volatile ("wfi");
    }

    if (handoff->version != DPOSLAB_HANDOFF_VERSION) {
        console_putln("HANDOFF VERSION              FAIL");
        console_present();
        for (;;)
            __asm__ volatile ("wfi");
    }

    if (handoff->size != sizeof(*handoff)) {
        console_putln("HANDOFF SIZE                 FAIL");
        console_present();
        for (;;)
            __asm__ volatile ("wfi");
    }

    if (handoff->stage2_sp == 0U) {
        console_putln("HANDOFF STAGE2 SP            FAIL");
        console_present();
        for (;;)
            __asm__ volatile ("wfi");
    }

    console_putln("HANDOFF ABI                  PASS");
    console_present();
    console_putln("STOCK API VALIDATE           RUNNING");
    console_present();
    if (!stock_api_validate()) { console_putln("STOCK API VALIDATE: FAIL"); console_present(); for(;;)__asm__ volatile("wfi"); }
    console_putln("STOCK API VALIDATE: OK"); console_present();
#if DPOSLAB_MODE == 0
    marker = 0x4C414210U;
    dposlab_trace_reset();

    console_putln("INSTALL FPGA RETURN HOOK    RUNNING");
    console_present();

    if (!dposlab_install_fpga_hook()) {
        marker = 0x4C4142FFU;
        console_putln("INSTALL FPGA RETURN HOOK    FAIL");
        console_present();

        for (;;)
            __asm__ volatile ("wfi");
    }

    marker = 0x4C414211U;
    console_putln("INSTALL FPGA RETURN HOOK    PASS");

    console_puts("STAGE2 SP                   ");
    put_hex(handoff->stage2_sp);
    console_putln("");

    console_putln("JUMP STOCK EXE2             NOW");
    console_present();

    stock_jump_exe2(handoff->stage2_sp);
#else
    {
        uint32_t scratch[8];
        uint32_t i;
        uint32_t zero_count = 0U;

        scratch[0] = 0xDEADBEEFU;
        scratch[1] = 0x13579BDFU;
        scratch[2] = 0xA5A5A5A5U;
        scratch[3] = 0x5A5A5A5AU;
        scratch[4] = 0x11223344U;
        scratch[5] = 0x55667788U;
        scratch[6] = 0xCAFEBABEU;
        scratch[7] = 0x0BADF00DU;

        marker = 0x4C414230U;

        console_putln("CALL STOCK ZERO FILL");
        line("ADDRESS: ", (uint32_t)(uintptr_t)scratch);
        line("SIZE:    ", (uint32_t)sizeof(scratch));
        line("BEFORE0: ", scratch[0]);
        line("BEFORE7: ", scratch[7]);
        console_present();

        stock_zero_fill(scratch, sizeof(scratch));

        marker = 0x4C414231U;

        for (i = 0U; i < 8U; ++i) {
            if (scratch[i] == 0U)
                zero_count += 1U;
        }

        console_clear();
        console_putln("DPOSLAB STOCK CALL POC");
        line("MARKER:     ", marker);
        line("AFTER0:     ", scratch[0]);
        line("AFTER7:     ", scratch[7]);
        line("ZERO COUNT: ", zero_count);

        if (zero_count == 8U)
            console_putln("STOCK CALL + RETURN: PASS");
        else
            console_putln("STOCK CALL + RETURN: FAIL");

        console_putln("POWER CYCLE TO EXIT");
        console_present();

        for (;;)
            __asm__ volatile ("wfi");
    }
#endif
}
