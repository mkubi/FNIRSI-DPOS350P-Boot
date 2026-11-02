#ifndef DPOSLAB_STOCK_ACQ_PROBE_H
#define DPOSLAB_STOCK_ACQ_PROBE_H
#include <stdint.h>

#define DPOSLAB_STOCK_BANK_COUNT 4U

struct dposlab_stock_bank_stats {
    uint32_t sentinel;
    uint32_t hash_before;
    uint32_t hash_after;
    uint32_t changed;
    uint32_t first_change;
    uint32_t last_change;
    uint32_t nonzero;
    uint32_t minimum;
    uint32_t maximum;
    uint32_t unique;
    uint32_t data0;
    uint32_t data1;
};

struct dposlab_stock_snapshot {
    uint32_t marker;
    uint32_t app_030;
    uint32_t app_034;
    uint32_t app_038;
    uint32_t app_290;
    uint32_t app_520;
    uint32_t app_63c;
    uint32_t app_6f2;
    uint32_t csi_mode;
    uint32_t csi_buffer0;
    uint32_t csi_irq_enable;
    uint32_t csi_status;
    uint32_t pe_cfg0;
    uint32_t pe_data;
};

struct dposlab_bounded_completion_diag {
    uint32_t attempted;
    uint32_t iterations;
    uint32_t exit_reason;
    uint32_t sw_done;
    uint32_t hw_done;
    uint32_t status_first;
    uint32_t status_last;
    uint32_t status_or;
    uint32_t mode_before;
    uint32_t mode_after_start;
    uint32_t mode_final;
    uint32_t buffer_expected;
    uint32_t buffer_actual;
    uint32_t irq_enable;
    uint32_t app_70e_before;
    uint32_t app_70e_after;
    uint32_t service_called;
    uint32_t status_before_service;
    uint32_t status_after_service;
    uint32_t app_70e_before_service;
    uint32_t app_70e_after_service;
    uint32_t service_acked_done;
    uint32_t service_published_done;
    uint32_t irq74_slot_before;
    uint32_t irq74_slot_after;
    uint32_t irq74_registered;
};

struct dposlab_stock_acq_result {
    uint32_t marker;
    struct dposlab_stock_snapshot s1_after_sysconfig;
    struct dposlab_stock_snapshot s2_sentinel_ready;
    struct dposlab_stock_snapshot s3_acq_entry;
    struct dposlab_stock_snapshot s5_acq_return;
    uint32_t status_before;
    uint32_t status_after;
    uint32_t ready;
    uint32_t period;
    uint32_t offset;
    uint32_t origin;
    uint32_t position;
    uint32_t raw_base;
    struct dposlab_bounded_completion_diag bounded;
    struct dposlab_stock_bank_stats bank[DPOSLAB_STOCK_BANK_COUNT];
};

extern volatile struct dposlab_stock_acq_result dposlab_stock_acq_result;
void dposlab_stock_acq_probe(void);
void dposlab_stock_acq_monitor(void);
#endif
