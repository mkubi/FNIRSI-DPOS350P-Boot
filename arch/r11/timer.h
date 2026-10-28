#ifndef R11_TIMER_H
#define R11_TIMER_H

#include <stdint.h>

void timer_init(void);
uint32_t timer_ticks(void);
uint32_t timer_get_ms(void);
void timer_delay_us(uint32_t microseconds);
void timer_delay_ms(uint32_t milliseconds);
void timer_avs_delay_units(uint32_t units);
int timer_deadline_reached(uint32_t deadline_ms);

#endif /* R11_TIMER_H */
