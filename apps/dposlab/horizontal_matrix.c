#include <stdint.h>

#include "horizontal_matrix.h"

#ifndef HORIZONTAL_H1
#define HORIZONTAL_H1 HORIZONTAL_IMPL_STOCK
#endif
#ifndef HORIZONTAL_H2
#define HORIZONTAL_H2 HORIZONTAL_IMPL_STOCK
#endif
#ifndef HORIZONTAL_H3
#define HORIZONTAL_H3 HORIZONTAL_IMPL_STOCK
#endif
#ifndef HORIZONTAL_H4
#define HORIZONTAL_H4 HORIZONTAL_IMPL_STOCK
#endif
#ifndef HORIZONTAL_H5
#define HORIZONTAL_H5 HORIZONTAL_IMPL_STOCK
#endif
#ifndef HORIZONTAL_H6
#define HORIZONTAL_H6 HORIZONTAL_IMPL_STOCK
#endif

#define STOCK_HORIZONTAL_H1 ((void (*)(void))(uintptr_t)0x4003A908U)
#define STOCK_HORIZONTAL_H2 ((void (*)(void))(uintptr_t)0x4003A680U)
#define STOCK_HORIZONTAL_H3 ((void (*)(void))(uintptr_t)0x4005DB9CU)
#define STOCK_HORIZONTAL_H4 ((void (*)(void))(uintptr_t)0x4005D27CU)
#define STOCK_HORIZONTAL_H5 ((void (*)(void))(uintptr_t)0x4003C008U)
#define STOCK_HORIZONTAL_H6 ((void (*)(void))(uintptr_t)0x4003BEB4U)

#define STOCK_APP_STATE_BASE 0x40544E68U

struct horizontal_h2_word {
    uint16_t offset;
    uint16_t reserved;
    uint32_t value;
};

#if HORIZONTAL_H2 == HORIZONTAL_IMPL_CUSTOM
/*
 * Exact word-level reproduction of stock 0x4003A680.
 *
 * This function only initializes the horizontal/timebase table in the stock
 * application state. It performs no FPGA, CSI or interrupt I/O.
 */
static void custom_horizontal_h2(void)
{
    static const struct horizontal_h2_word words[] = {
        { 0x0BD0U, 0U, 0x00000001U },
        { 0x0BD4U, 0U, 0x00000000U },
        { 0x0BD8U, 0U, 0x00000002U },
        { 0x0BDCU, 0U, 0x00000000U },
        { 0x0BE0U, 0U, 0x00000005U },
        { 0x0BE4U, 0U, 0x00000000U },
        { 0x0BE8U, 0U, 0x0000000AU },
        { 0x0BECU, 0U, 0x00000000U },
        { 0x0BF0U, 0U, 0x00000014U },
        { 0x0BF4U, 0U, 0x00000000U },
        { 0x0BF8U, 0U, 0x00000032U },
        { 0x0BFCU, 0U, 0x00000000U },
        { 0x0C00U, 0U, 0x00000064U },
        { 0x0C04U, 0U, 0x00000000U },
        { 0x0C08U, 0U, 0x000000C8U },
        { 0x0C0CU, 0U, 0x00000000U },
        { 0x0C10U, 0U, 0x000001F4U },
        { 0x0C14U, 0U, 0x00000000U },
        { 0x0C18U, 0U, 0x000003E8U },
        { 0x0C1CU, 0U, 0x00000000U },
        { 0x0C20U, 0U, 0x000007D0U },
        { 0x0C24U, 0U, 0x00000000U },
        { 0x0C28U, 0U, 0x004C4B40U },
        { 0x0C2CU, 0U, 0x00000000U },
        { 0x0C30U, 0U, 0x00001388U },
        { 0x0C34U, 0U, 0x00000000U },
        { 0x0C38U, 0U, 0x00002710U },
        { 0x0C3CU, 0U, 0x00000000U },
        { 0x0C40U, 0U, 0x00004E20U },
        { 0x0C44U, 0U, 0x00000000U },
        { 0x0C48U, 0U, 0x0000C350U },
        { 0x0C4CU, 0U, 0x00000000U },
        { 0x0C50U, 0U, 0x000186A0U },
        { 0x0C54U, 0U, 0x00000000U },
        { 0x0C58U, 0U, 0x00030D40U },
        { 0x0C5CU, 0U, 0x00000000U },
        { 0x0C60U, 0U, 0x0007A120U },
        { 0x0C64U, 0U, 0x00000000U },
        { 0x0C68U, 0U, 0x000F4240U },
        { 0x0C6CU, 0U, 0x00000000U },
        { 0x0C70U, 0U, 0x001E8480U },
        { 0x0C74U, 0U, 0x00000000U },
        { 0x0C78U, 0U, 0x004C4B40U },
        { 0x0C7CU, 0U, 0x00000000U },
        { 0x0C80U, 0U, 0x00989680U },
        { 0x0C84U, 0U, 0x00000000U },
        { 0x0C88U, 0U, 0x01312D00U },
        { 0x0C8CU, 0U, 0x00000000U },
        { 0x0C90U, 0U, 0x02FAF080U },
        { 0x0C94U, 0U, 0x00000000U },
        { 0x0C98U, 0U, 0x05F5E100U },
        { 0x0C9CU, 0U, 0x00000000U },
        { 0x0CA0U, 0U, 0x0BEBC200U },
        { 0x0CA4U, 0U, 0x00000000U },
        { 0x0CA8U, 0U, 0x1DCD6500U },
        { 0x0CACU, 0U, 0x00000000U },
        { 0x0CB0U, 0U, 0x3B9ACA00U },
        { 0x0CB4U, 0U, 0x00000000U },
        { 0x0CB8U, 0U, 0x77359400U },
        { 0x0CBCU, 0U, 0x00000000U },
        { 0x0CC0U, 0U, 0x2A05F200U },
        { 0x0CC4U, 0U, 0x00000001U },
        { 0x0CC8U, 0U, 0x540BE400U },
        { 0x0CCCU, 0U, 0x00000002U },
        { 0x0CD0U, 0U, 0xA817C800U },
        { 0x0CD4U, 0U, 0x00000004U },
        { 0x0CD8U, 0U, 0xA43B7400U },
        { 0x0CDCU, 0U, 0x0000000BU },
        { 0x0CE0U, 0U, 0x4876E800U },
        { 0x0CE4U, 0U, 0x00000017U },
        { 0x0CE8U, 0U, 0x90EDD000U },
        { 0x0CECU, 0U, 0x0000002EU },
        { 0x0CF0U, 0U, 0x6A528800U },
        { 0x0CF4U, 0U, 0x00000074U },
        { 0x0CF8U, 0U, 0xD4A51000U },
        { 0x0CFCU, 0U, 0x000000E8U },
        { 0x0D00U, 0U, 0xA94A2000U },
        { 0x0D04U, 0U, 0x000001D1U },
        { 0x0D08U, 0U, 0x27395000U },
        { 0x0D0CU, 0U, 0x0000048CU },
        { 0x0D10U, 0U, 0x4E72A000U },
        { 0x0D14U, 0U, 0x00000918U },
        { 0x0D18U, 0U, 0x9CE54000U },
        { 0x0D1CU, 0U, 0x00001230U },
        { 0x0D20U, 0U, 0x883D2000U },
        { 0x0D24U, 0U, 0x00002D79U },
    };
    uint32_t i;

    for (i = 0U; i < sizeof(words) / sizeof(words[0]); ++i) {
        *(volatile uint32_t *)(uintptr_t)
            (STOCK_APP_STATE_BASE + words[i].offset) = words[i].value;
    }

    __asm__ volatile ("dmb sy" ::: "memory");
}
#endif

#if HORIZONTAL_H1 == HORIZONTAL_IMPL_CUSTOM || \
    HORIZONTAL_H3 == HORIZONTAL_IMPL_CUSTOM || \
    HORIZONTAL_H4 == HORIZONTAL_IMPL_CUSTOM || \
    HORIZONTAL_H5 == HORIZONTAL_IMPL_CUSTOM || \
    HORIZONTAL_H6 == HORIZONTAL_IMPL_CUSTOM
#error "Only Horizontal H2 has a CUSTOM implementation"
#endif

#if HORIZONTAL_H1 != HORIZONTAL_IMPL_STOCK && \
    HORIZONTAL_H1 != HORIZONTAL_IMPL_SKIP
#error "HORIZONTAL_H1 supports STOCK or SKIP"
#endif
#if HORIZONTAL_H2 != HORIZONTAL_IMPL_STOCK && \
    HORIZONTAL_H2 != HORIZONTAL_IMPL_CUSTOM && \
    HORIZONTAL_H2 != HORIZONTAL_IMPL_SKIP
#error "HORIZONTAL_H2 supports STOCK, CUSTOM or SKIP"
#endif
#if HORIZONTAL_H3 != HORIZONTAL_IMPL_STOCK && \
    HORIZONTAL_H3 != HORIZONTAL_IMPL_SKIP
#error "HORIZONTAL_H3 supports STOCK or SKIP"
#endif
#if HORIZONTAL_H4 != HORIZONTAL_IMPL_STOCK && \
    HORIZONTAL_H4 != HORIZONTAL_IMPL_SKIP
#error "HORIZONTAL_H4 supports STOCK or SKIP"
#endif
#if HORIZONTAL_H5 != HORIZONTAL_IMPL_STOCK && \
    HORIZONTAL_H5 != HORIZONTAL_IMPL_SKIP
#error "HORIZONTAL_H5 supports STOCK or SKIP"
#endif
#if HORIZONTAL_H6 != HORIZONTAL_IMPL_STOCK && \
    HORIZONTAL_H6 != HORIZONTAL_IMPL_SKIP
#error "HORIZONTAL_H6 supports STOCK or SKIP"
#endif

void dposlab_horizontal_matrix_init(void)
{
#if HORIZONTAL_H1 == HORIZONTAL_IMPL_STOCK
    STOCK_HORIZONTAL_H1();
#endif
#if HORIZONTAL_H2 == HORIZONTAL_IMPL_STOCK
    STOCK_HORIZONTAL_H2();
#elif HORIZONTAL_H2 == HORIZONTAL_IMPL_CUSTOM
    custom_horizontal_h2();
#endif
#if HORIZONTAL_H3 == HORIZONTAL_IMPL_STOCK
    STOCK_HORIZONTAL_H3();
#endif
#if HORIZONTAL_H4 == HORIZONTAL_IMPL_STOCK
    STOCK_HORIZONTAL_H4();
#endif
#if HORIZONTAL_H5 == HORIZONTAL_IMPL_STOCK
    STOCK_HORIZONTAL_H5();
#endif
#if HORIZONTAL_H6 == HORIZONTAL_IMPL_STOCK
    STOCK_HORIZONTAL_H6();
#endif
}
