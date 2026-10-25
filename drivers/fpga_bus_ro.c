#include "fpga_bus_ro.h"

#define PIO_BASE       0x01C20800u
#define PE_CFG0        (*(volatile uint32_t *)(PIO_BASE + 0x90u))
#define PE_CFG1        (*(volatile uint32_t *)(PIO_BASE + 0x94u))
#define PE_DATA        (*(volatile uint32_t *)(PIO_BASE + 0xA0u))

#define FPGA_MAGIC     0xA5A55A5Au
#define PE_CTRL_MASK   0x0000000Fu
#define PE_DATA_MASK   0x0000FF00u

static inline void barrier(void)
{
    __asm__ volatile ("dsb sy" ::: "memory");
}

static inline void pe_set(unsigned pin)
{
    PE_DATA |= 1u << pin;
    barrier();
}

static inline void pe_clear(unsigned pin)
{
    PE_DATA &= ~(1u << pin);
    barrier();
}

static void data_bus_input(void)
{
    /* PE8..PE15: function 0 = GPIO input. */
    PE_CFG1 = 0x00000000u;
    barrier();
}

static void data_bus_output(void)
{
    /* PE8..PE15: function 1 = GPIO output. */
    PE_CFG1 = 0x11111111u;
    barrier();
}

static void write_data_byte(uint8_t value)
{
    uint32_t data = PE_DATA;
    data &= ~PE_DATA_MASK;
    data |= (uint32_t)value << 8;
    PE_DATA = data;
    barrier();
}

static void control_bus_init(void)
{
    uint32_t cfg = PE_CFG0;

    /* PE0..PE3: GPIO outputs. Preserve PE4..PE7. */
    cfg &= ~0x0000FFFFu;
    cfg |=  0x00001111u;
    PE_CFG0 = cfg;
    barrier();

    /* EXE2 0x40049110 idle state: PE0 high, PE1 low. */
    PE_DATA = (PE_DATA & ~PE_CTRL_MASK) | 0x00000001u;
    barrier();
}


static void fpga_select_baseline(uint8_t value)
{
    data_bus_output();

    /* Previously verified diagnostic selector: PE2=1, PE3=1, PE0 0->1. */
    pe_set(2);
    pe_set(3);
    write_data_byte(value);
    pe_clear(0);
    pe_set(0);
}

static void fpga_select_command(uint8_t value)
{
    data_bus_output();

    /* Exact EXE2 0x4004927C primitive: PE2=0, PE3=0, data, PE0 1->0. */
    pe_clear(2);
    pe_clear(3);
    write_data_byte(value);
    pe_set(0);
    pe_clear(0);
}

static void fpga_write_payload(uint8_t value)
{
    data_bus_output();

    /* Exact EXE2 0x40049214 primitive: PE2=0, PE3=1, data, PE0 1->0. */
    pe_clear(2);
    pe_set(3);
    write_data_byte(value);
    pe_set(0);
    pe_clear(0);
}

static void fpga_settle_delay(void)
{
    /* Deliberately short bus-settle delay. This is not a millisecond delay.
       The volatile loop prevents -O2 from removing the timing gap. */
    volatile unsigned i;
    for (i = 0; i < 64U; ++i)
        __asm__ volatile ("nop");
    barrier();
}

static uint8_t fpga_read_byte(void)
{
    uint8_t value;

    data_bus_input();

    /* EXE2 0x400491B8 path. */
    pe_clear(2);
    pe_clear(3);
    pe_clear(0);
    value = (uint8_t)((PE_DATA >> 8) & 0xFFu);
    pe_set(0);

    return value;
}

void fpga_bus_safe_idle(void)
{
    data_bus_input();
    control_bus_init();
}

void fpga_diag_read_id(struct fpga_diag_result *result)
{
    unsigned i;
    uint32_t word = 0;
    uint32_t shifted = 0;

    result->pe_cfg0_before = PE_CFG0;
    result->pe_cfg1_before = PE_CFG1;
    result->pe_data_before = PE_DATA;

    control_bus_init();
    fpga_select_command(1u);
    fpga_settle_delay();

    /* Read one extra byte. If the first cycle is a dummy/stale byte, the
       expected magic will appear in raw[1]..raw[4]. */
    for (i = 0; i < 5; ++i)
        result->raw[i] = fpga_read_byte();

    for (i = 0; i < 4; ++i)
        word = (word << 8) | result->raw[i];
    for (i = 1; i < 5; ++i)
        shifted = (shifted << 8) | result->raw[i];

    result->id_word = word;
    result->id_word_shifted = shifted;
    result->present = (word == FPGA_MAGIC) || (shifted == FPGA_MAGIC);

    /* Restore the original bus-idle convention: data input, PE0 high,
       PE1 low. PE2/PE3 are deasserted high after the transaction. */
    data_bus_input();
    pe_set(0);
    pe_clear(1);
    pe_set(2);
    pe_set(3);

    result->pe_cfg0_after = PE_CFG0;
    result->pe_cfg1_after = PE_CFG1;
    result->pe_data_after = PE_DATA;
}


void fpga_capture_arm_probe(struct fpga_capture_arm_probe_result *result)
{
    result->pe_cfg0_before = PE_CFG0;
    result->pe_cfg1_before = PE_CFG1;
    result->pe_data_before = PE_DATA;

    control_bus_init();

    /* Confirmed EXE2 0x4003F8E0 sequence. */
    fpga_select_command(0x08U);
    fpga_settle_delay();
    fpga_write_payload(0x00U);
    fpga_settle_delay();
    fpga_select_command(0x08U);
    fpga_settle_delay();
    fpga_write_payload(0x01U);
    fpga_settle_delay();

    /* Leave the GPIO bus in the same safe idle convention as ID probe. */
    fpga_bus_safe_idle();

    result->pe_cfg0_after = PE_CFG0;
    result->pe_cfg1_after = PE_CFG1;
    result->pe_data_after = PE_DATA;
}


void fpga_select_bank0(void)
{
    control_bus_init();
    fpga_select_command(0x15U);
    fpga_settle_delay();
    fpga_write_payload(0x00U);
    fpga_settle_delay();
    fpga_bus_safe_idle();
}


void fpga_diag_read_id_baseline(struct fpga_diag_result *result)
{
    unsigned i;
    uint32_t word = 0U;
    uint32_t shifted = 0U;

    result->pe_cfg0_before = PE_CFG0;
    result->pe_cfg1_before = PE_CFG1;
    result->pe_data_before = PE_DATA;

    control_bus_init();
    fpga_select_baseline(0x01U);
    fpga_settle_delay();

    for (i = 0U; i < 5U; ++i)
        result->raw[i] = fpga_read_byte();

    for (i = 0U; i < 4U; ++i)
        word = (word << 8) | result->raw[i];
    for (i = 1U; i < 5U; ++i)
        shifted = (shifted << 8) | result->raw[i];

    result->id_word = word;
    result->id_word_shifted = shifted;
    result->present = (word == FPGA_MAGIC) || (shifted == FPGA_MAGIC);

    data_bus_input();
    pe_set(0);
    pe_clear(1);
    pe_set(2);
    pe_set(3);

    result->pe_cfg0_after = PE_CFG0;
    result->pe_cfg1_after = PE_CFG1;
    result->pe_data_after = PE_DATA;
}
