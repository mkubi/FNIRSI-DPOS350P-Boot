#ifndef GPIO_H
#define GPIO_H
#include <stdint.h>
#define GPIO_PORT_B 0x24U
#define GPIO_PORT_C 0x48U
#define GPIO_PORT_D 0x6CU
#define GPIO_PORT_E 0x90U
#define GPIO_PORT_G 0xD8U
void gpio_set_function(uint32_t port, unsigned int pin, unsigned int function);
void gpio_set_output(uint32_t port, unsigned int pin);
void gpio_set_input(uint32_t port, unsigned int pin);
void gpio_write(uint32_t port, unsigned int pin, int high);
int gpio_read(uint32_t port, unsigned int pin);
#endif
