#ifndef DPOSLAB_CUSTOM_FPGA_START_H
#define DPOSLAB_CUSTOM_FPGA_START_H

#include <stdint.h>

#define FPGA_IMPL_STOCK   0
#define FPGA_IMPL_CUSTOM  1
#define FPGA_IMPL_SKIP    2
#define FPGA_IMPL_EXACT   3

#ifndef FPGA_F1
#define FPGA_F1 FPGA_IMPL_STOCK
#endif
#ifndef FPGA_F2
#define FPGA_F2 FPGA_IMPL_EXACT
#endif
#ifndef FPGA_F3
#define FPGA_F3 FPGA_IMPL_CUSTOM
#endif
#ifndef FPGA_F4
#define FPGA_F4 FPGA_IMPL_CUSTOM
#endif

struct dposlab_custom_fpga_start_result {
    uint32_t id;
    uint32_t pe_cfg0;
    uint32_t pe_cfg1;
    uint32_t pe_data;
    uint32_t csi_if_cfg;
    uint32_t csi_mode;
    uint32_t csi_hsize;
    uint32_t csi_buffer0;
    uint32_t csi_irq_enable;
    uint32_t clock_writes;
    uint8_t bus_direction_state;
    uint8_t interface_mode_state;
    uint8_t f1;
    uint8_t f2;
    uint8_t f3;
    uint8_t f4;
    int id_ok;
    int csi_ok;
    int clock_ok;
    int success;
};

extern volatile struct dposlab_custom_fpga_start_result
    dposlab_custom_fpga_start_result;

void dposlab_custom_fpga_start(void);
const char *dposlab_fpga_impl_name(unsigned impl);

#endif
