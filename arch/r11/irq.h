#ifndef FAT_DIAG_IRQ_H
#define FAT_DIAG_IRQ_H

void irq_controller_init(void);
void timer0_tick_init(void);
void timer0_tick_isr(void);
void irq_enable_global(void);
void irq_register_handler(uint32_t irq, uintptr_t handler, uint32_t priority, uint32_t enable);

#endif
