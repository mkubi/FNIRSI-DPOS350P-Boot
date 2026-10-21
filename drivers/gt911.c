#include "gt911.h"
int gt911_init(void){return 0;}
int gt911_read(gt911_touch_t *touch){if(!touch)return -1;touch->x=0;touch->y=0;touch->pressed=0;touch->points=0;return 0;}
