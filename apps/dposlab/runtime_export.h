#ifndef DPOSLAB_RUNTIME_EXPORT_H
#define DPOSLAB_RUNTIME_EXPORT_H

#include <stdint.h>

struct dposlab_export_status {
    uint32_t attempts;
    uint32_t successes;
    uint32_t bytes_requested;
    uint32_t bytes_written;
    int32_t validation_result;
    int32_t open_result;
    int32_t write_result;
    int32_t close_result;
};

extern volatile struct dposlab_export_status dposlab_export_status;

void dposlab_trace_export_after_screenshot(void);

#endif
