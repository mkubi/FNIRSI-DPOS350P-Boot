#ifndef GT911_H
#define GT911_H

#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t pressed;
    uint8_t points;
} gt911_touch_t;

typedef struct {
    char product_id[5];
    uint8_t address;
    uint8_t available;
} gt911_info_t;

typedef struct {
    gt911_touch_t touch;
    uint8_t raw_status;
    uint8_t int_level;
    uint8_t data_ready;
} gt911_debug_sample_t;

int gt911_init(gt911_info_t *info);
int gt911_poll(gt911_touch_t *touch);

/* Diagnostic read: exposes status and INT without hiding useful evidence. */
int gt911_debug_sample(gt911_debug_sample_t *sample, int clear_status);

#endif /* GT911_H */

