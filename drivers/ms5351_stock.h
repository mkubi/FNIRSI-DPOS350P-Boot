#ifndef MS5351_STOCK_H
#define MS5351_STOCK_H

#include <stdint.h>

struct ms5351_stock_result {
    uint32_t writes;
    int completed;
};

/* Fixed stock EXE2 sequence for 95 MHz CLK0 and 10 MHz CLK1. */
void ms5351_stock_init_95m_10m(struct ms5351_stock_result *result);

#endif
