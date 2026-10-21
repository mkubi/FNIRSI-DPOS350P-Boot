#ifndef SPINOR_H
#define SPINOR_H

#include <stdint.h>

int spinor_init(void);
int spinor_read(uint32_t offset, void *buffer, uint32_t length);
void spinor_deinit(void);

#endif
