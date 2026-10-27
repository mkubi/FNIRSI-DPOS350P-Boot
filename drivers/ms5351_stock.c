#include "ms5351_stock.h"

#define PE_CFG2  (*(volatile uint32_t *)0x01C20898U)
#define PE_DATA  (*(volatile uint32_t *)0x01C208A0U)
#define SCL_PIN  20U
#define SDA_PIN  21U

static void short_delay(void)
{
    volatile unsigned i;
    for (i = 0U; i < 24U; ++i)
        __asm__ volatile ("nop");
}

static void pin_high(unsigned pin)
{
    PE_DATA |= 1U << pin;
    short_delay();
}

static void pin_low(unsigned pin)
{
    PE_DATA &= ~(1U << pin);
    short_delay();
}

static void bus_start(void)
{
    pin_high(SDA_PIN);
    pin_high(SCL_PIN);
    pin_low(SDA_PIN);
    pin_low(SCL_PIN);
}

static void bus_stop(void)
{
    pin_low(SCL_PIN);
    pin_low(SDA_PIN);
    pin_high(SCL_PIN);
    pin_high(SDA_PIN);
}

static void write_byte(uint8_t value)
{
    unsigned bit;
    for (bit = 0U; bit < 8U; ++bit) {
        pin_low(SCL_PIN);
        if ((value & 0x80U) != 0U)
            pin_high(SDA_PIN);
        else
            pin_low(SDA_PIN);
        pin_high(SCL_PIN);
        value <<= 1;
    }
    /* Stock driver does not sample ACK; it emits the ninth clock. */
    pin_low(SCL_PIN);
    pin_high(SDA_PIN);
    pin_high(SCL_PIN);
    pin_low(SCL_PIN);
}

static void write_reg(uint8_t reg, uint8_t value, uint32_t *count)
{
    bus_start();
    write_byte(0xC0U);
    write_byte(reg);
    write_byte(value);
    bus_stop();
    ++*count;
}

static void write_block(uint8_t first, const uint8_t values[8], uint32_t *count)
{
    unsigned i;
    for (i = 0U; i < 8U; ++i)
        write_reg((uint8_t)(first + i), values[i], count);
}

void ms5351_stock_init_95m_10m(struct ms5351_stock_result *result)
{
    static const uint8_t pll_760m[8] = {
        0xFFU, 0xFFU, 0x00U, 0x0DU, 0x33U, 0xF3U, 0x33U, 0x33U
    };
    static const uint8_t pllb_900m[8] = {
        0xFFU, 0xFFU, 0x00U, 0x10U, 0x00U, 0xF0U, 0x00U, 0x00U
    };
    static const uint8_t ms0_div8[8] = {
        0x00U, 0x01U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U
    };
    static const uint8_t ms1_div90[8] = {
        0x00U, 0x01U, 0x00U, 0x2BU, 0x00U, 0x00U, 0x00U, 0x00U
    };
    uint32_t count = 0U;

    result->writes = 0U;
    result->completed = 0;

    /* Stock 0x4005F1BC: PE20/PE21 as GPIO outputs. */
    /*
     * Stock 0x4005F1BC configures PE20 and PE21 as GPIO outputs:
     * function 1 in PE_CFG2, not function 0/input.
     */
    PE_CFG2 =
        (PE_CFG2 & ~((0xFU << 16) | (0xFU << 20))) |
        (0x1U << 16) |
        (0x1U << 20);

    pin_high(SCL_PIN);
    pin_high(SDA_PIN);

    /* Stock 0x4005F1F0(95 MHz): PLLA, MS0, reset PLLA, enable CLK0. */
    write_block(0x1AU, pll_760m, &count);
    write_block(0x2AU, ms0_div8, &count);
    write_reg(0xB1U, 0x20U, &count);
    write_reg(0x10U, 0x4FU, &count);

    /* Stock 0x4005F38C(10 MHz): PLLB is 36 + 0/0xFFFFF = 900 MHz.
     * The stock helper receives base register 0x22, not PLLA base 0x1A. */
    write_block(0x22U, pllb_900m, &count);
    write_block(0x32U, ms1_div90, &count);
    write_reg(0xB1U, 0x80U, &count);
    write_reg(0x11U, 0x6FU, &count);

    result->writes = count;
    result->completed = 1;
}
