#include <stdint.h>

#include "sysconfig_group_matrix.h"

typedef void (*stock_void_fn_t)(void);

static void call_stock(uint32_t address)
{
    ((stock_void_fn_t)(uintptr_t)address)();
}

void __attribute__((noinline)) dposlab_sysconfig_group_dispatch(void)
{
#if SYSCFG_G1 == SYSCFG_GROUP_STOCK
    /*
     * Stock SC1 @ 0x4003A654 decomposed without diagnostic side effects.
     * Preserve the original helper order exactly.
     */
#if SYSCFG_SC1_1 == SYSCFG_GROUP_STOCK
    call_stock(0x4003BE78U);
#endif
#if SYSCFG_SC1_2 == SYSCFG_GROUP_STOCK
    call_stock(0x4003A908U);
#endif
#if SYSCFG_SC1_3 == SYSCFG_GROUP_STOCK
    call_stock(0x4003A984U);
#endif
#if SYSCFG_SC1_4 == SYSCFG_GROUP_STOCK
    /* Stock SC1 loads r0/r1 from APP_STATE +0x30 before this call. */
    {
        typedef void (*stock_sc1_4_fn_t)(uint32_t, uint32_t);
        volatile const uint32_t *state =
            (volatile const uint32_t *)(uintptr_t)(0x40544E68U + 0x30U);
        ((stock_sc1_4_fn_t)(uintptr_t)0x4003F948U)(state[0], state[1]);
    }
#endif
#if SYSCFG_SC1_5 == SYSCFG_GROUP_STOCK
    call_stock(0x4003F894U);
#endif
#endif
#if SYSCFG_G2 == SYSCFG_GROUP_STOCK
    call_stock(0x4004A510U);
#endif
#if SYSCFG_G3 == SYSCFG_GROUP_STOCK
    call_stock(0x40063328U);
#endif
}
