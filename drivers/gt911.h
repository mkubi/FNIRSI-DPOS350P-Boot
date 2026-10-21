#ifndef GT911_H
#define GT911_H
#include <stdint.h>
typedef struct { uint16_t x,y; uint8_t pressed,points; } gt911_touch_t;
int gt911_init(void); int gt911_read(gt911_touch_t *touch);
#endif
