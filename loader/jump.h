#ifndef LOADER_JUMP_H
#define LOADER_JUMP_H

#include <stdint.h>

void loader_jump(uint32_t entry_address) __attribute__((noreturn));

#endif /* LOADER_JUMP_H */
