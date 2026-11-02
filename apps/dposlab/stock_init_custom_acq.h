#ifndef DPOSLAB_STOCK_INIT_CUSTOM_ACQ_H
#define DPOSLAB_STOCK_INIT_CUSTOM_ACQ_H

#include <stdint.h>

#define DPOSLAB_ORACLE_BANK_COUNT 4U

struct dposlab_oracle_bank_stats {
    uint32_t done;
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

struct dposlab_stock_init_custom_acq_result {
    uint32_t marker;
    uint32_t sysconfig_called;
    uint32_t status_before;
    uint32_t status_after;
    uint32_t polls;
    uint32_t ready;
    uint32_t pointer_valid;
    uint32_t live_pointer;
    uint32_t runtime_290;
    uint32_t runtime_63c;
    uint32_t capture_position;
    uint32_t raw_base;
    struct dposlab_oracle_bank_stats bank[DPOSLAB_ORACLE_BANK_COUNT];
};

extern volatile struct dposlab_stock_init_custom_acq_result
    dposlab_stock_init_custom_acq_result;

void dposlab_stock_init_custom_acq_probe(void);
void dposlab_stock_init_custom_acq_monitor(void);

#endif
