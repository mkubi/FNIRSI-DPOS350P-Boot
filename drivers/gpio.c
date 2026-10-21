#include "gpio.h"
#include "regs.h"

static uintptr_t cfg_addr(uint32_t port, unsigned int pin)
{
    return R11_PIO_BASE + port + (pin / 8U) * 4U;
}

static uintptr_t data_addr(uint32_t port)
{
    return R11_PIO_BASE + port + 0x10U;
}

void gpio_set_function(uint32_t port, unsigned int pin,
                       unsigned int function)
{
    uintptr_t addr = cfg_addr(port, pin);
    unsigned int shift = (pin % 8U) * 4U;
    uint32_t value = mmio_read32(addr);

    value &= ~(0xFU << shift);
    value |= (function & 0xFU) << shift;
    mmio_write32(addr, value);
}

void gpio_set_output(uint32_t port, unsigned int pin)
{
    gpio_set_function(port, pin, 1U);
}

void gpio_set_input(uint32_t port, unsigned int pin)
{
    gpio_set_function(port, pin, 0U);
}

void gpio_write(uint32_t port, unsigned int pin, int high)
{
    uintptr_t addr = data_addr(port);
    uint32_t value = mmio_read32(addr);

    if (high)
        value |= 1U << pin;
    else
        value &= ~(1U << pin);

    mmio_write32(addr, value);
}

int gpio_read(uint32_t port, unsigned int pin)
{
    return (int)((mmio_read32(data_addr(port)) >> pin) & 1U);
}
