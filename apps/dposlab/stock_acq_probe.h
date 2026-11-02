#ifndef DPOSLAB_STOCK_ACQ_PROBE_H
#define DPOSLAB_STOCK_ACQ_PROBE_H

#include <stdint.h>

struct dposlab_stock_bank_stats {
    uint32_t nonzero;
    uint32_t first_nonzero;
    uint32_t last_nonzero;
    uint32_t minimum;
    uint32_t maximum;
    uint32_t unique;
    uint32_t hash;
    uint32_t data0;
    uint32_t data1;
};

struct dposlab_stock_acq_result {
    uint32_t status_before;
    uint32_t status_after;
    uint32_t ready;
    uint32_t period;
    uint32_t offset;
    uint32_t origin;
    uint32_t position;
    uint32_t raw_base;
    struct dposlab_stock_bank_stats bank[4];
};

extern volatile struct dposlab_stock_acq_result dposlab_stock_acq_result;

void dposlab_stock_acq_probe(void);
void dposlab_stock_acq_monitor(void);

#endif
