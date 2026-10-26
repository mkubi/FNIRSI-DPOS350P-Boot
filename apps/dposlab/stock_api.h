#ifndef DPOSLAB_STOCK_API_H
#define DPOSLAB_STOCK_API_H
#include <stdint.h>
#define STOCK_EXE2_ENTRY  0x40000000U
#define STOCK_FPGA_START  0x40049188U
#define STOCK_ZERO_FILL   0x40068E5CU
int stock_api_validate(void);
void stock_call_fpga_start(void);
void stock_zero_fill(void *destination, uint32_t size);
void stock_jump_exe2(uint32_t stage2_sp)
    __attribute__((noreturn));
#endif
