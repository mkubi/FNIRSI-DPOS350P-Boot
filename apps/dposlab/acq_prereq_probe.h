#ifndef DPOSLAB_ACQ_PREREQ_PROBE_H
#define DPOSLAB_ACQ_PREREQ_PROBE_H

#include <stdint.h>

struct dposlab_acq_probe_result {
    uint32_t factory_magic0;
    uint32_t factory_magic1;
    uint32_t factory_magic2;
    uint32_t factory_magic3;
    uint32_t mode_byte;
    uint32_t raw0a;
    uint32_t prepared0a;
    uint32_t polls;
    uint32_t ready;
    uint32_t pe_cfg0;
    uint32_t pe_cfg1;
    uint32_t pe_data;
};

extern struct dposlab_acq_probe_result dposlab_acq_probe_result;
void dposlab_acq_variant1_probe(void);
void dposlab_acq_variant1_monitor(void);

#endif
