#ifndef DPOSLAB_STOCK_PRIMITIVE_SMEC_TEST_H
#define DPOSLAB_STOCK_PRIMITIVE_SMEC_TEST_H

#include "../../drivers/fpga_bus_ro.h"

/*
 * Test A1:
 *   Custom transaction/retry algorithm
 *   over exact stock FPGA selector/write/read primitives.
 */
void dposlab_custom_smec_stock_primitives_test(
    struct fpga_smec_stage24_pe_result *result);

#endif
