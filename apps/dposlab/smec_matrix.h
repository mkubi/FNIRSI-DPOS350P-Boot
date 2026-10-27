#ifndef DPOSLAB_SMEC_MATRIX_H
#define DPOSLAB_SMEC_MATRIX_H

#include <stdint.h>
#include "../../drivers/fpga_bus_ro.h"

#define SMEC_IMPL_STOCK   0
#define SMEC_IMPL_CUSTOM  1
#define SMEC_IMPL_SKIP    2
#define SMEC_IMPL_EXACT   3

#ifndef SMEC_S1
#define SMEC_S1 SMEC_IMPL_EXACT
#endif
#ifndef SMEC_S2
#define SMEC_S2 SMEC_IMPL_STOCK
#endif
#ifndef SMEC_S3
#define SMEC_S3 SMEC_IMPL_CUSTOM
#endif
#ifndef SMEC_S4
#define SMEC_S4 SMEC_IMPL_CUSTOM
#endif

struct dposlab_smec_matrix_result {
    struct fpga_smec_init_result custom;
    uint32_t state_checksum;
    uint32_t state_nonzero;
    uint8_t s1;
    uint8_t s2;
    uint8_t s3;
    uint8_t s4;
    int success;
};

extern volatile struct dposlab_smec_matrix_result dposlab_smec_matrix_result;
void dposlab_smec_matrix_init(void);
const char *dposlab_smec_impl_name(unsigned impl);

#endif
