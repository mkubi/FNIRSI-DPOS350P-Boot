#include "stock_api.h"
#define MMIO32(a) (*(volatile const uint32_t *)(uintptr_t)(a))
typedef void (*stock_void_fn_t)(void);
typedef void (*stock_zero_fill_fn_t)(void *destination, uint32_t size);
int stock_api_validate(void)
{
    return MMIO32(STOCK_FPGA_START) == 0xE92D4000U &&
           MMIO32(STOCK_EXE2_ENTRY) == 0xE59FF018 &&
           MMIO32(STOCK_ZERO_FILL) == 0xE3B02000U;
}
void stock_call_fpga_start(void)
{
    ((stock_void_fn_t)(uintptr_t)STOCK_FPGA_START)();
}

void stock_zero_fill(void *destination, uint32_t size)
{
    ((stock_zero_fill_fn_t)(uintptr_t)STOCK_ZERO_FILL)(
        destination,
        size);
}
extern void stock_jump_exe2_with_sp(uint32_t entry, uint32_t stage2_sp)
    __attribute__((noreturn));

void stock_jump_exe2(uint32_t stage2_sp)
{
    stock_jump_exe2_with_sp(STOCK_EXE2_ENTRY, stage2_sp);
}
