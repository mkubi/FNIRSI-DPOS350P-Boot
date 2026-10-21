#include "gpio.h"
#include "regs.h"
static uintptr_t cfg_addr(uint32_t port, unsigned int pin) { return R11_PIO_BASE + port + (pin / 8U) * 4U; }
static uintptr_t data_addr(uint32_t port) { return R11_PIO_BASE + port + 0x10U; }
void gpio_set_function(uint32_t port, unsigned int pin, unsigned int function) {
    uintptr_t a=cfg_addr(port,pin); unsigned int sh=(pin%8U)*4U; uint32_t v=mmio_read32(a);
    v &= ~(0x7U<<sh); v |= (function&0x7U)<<sh; mmio_write32(a,v);
}
void gpio_set_output(uint32_t p,unsigned int n){gpio_set_function(p,n,1U);} void gpio_set_input(uint32_t p,unsigned int n){gpio_set_function(p,n,0U);}
void gpio_write(uint32_t p,unsigned int n,int high){uintptr_t a=data_addr(p);uint32_t v=mmio_read32(a);if(high)v|=1U<<n;else v&=~(1U<<n);mmio_write32(a,v);} int gpio_read(uint32_t p,unsigned int n){return (int)((mmio_read32(data_addr(p))>>n)&1U);}
