#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdint.h>
#define DISPLAY_WIDTH  1024U
#define DISPLAY_HEIGHT 600U

/*
 * DPOS350P framebuffer pixel format, hardware-confirmed: BGR565.
 *   bits 15:11 = blue
 *   bits 10:5  = green
 *   bits 4:0   = red
 */
#define DISPLAY_COLOR_BLACK   0x0000U
#define DISPLAY_COLOR_WHITE   0xFFFFU
#define DISPLAY_COLOR_RED     0x001FU
#define DISPLAY_COLOR_GREEN   0x07E0U
#define DISPLAY_COLOR_BLUE    0xF800U
#define DISPLAY_COLOR_CYAN    0xFFE0U
#define DISPLAY_COLOR_YELLOW  0x07FFU
#define DISPLAY_COLOR_GRAY    0x8410U

/* Backward-compatible names for existing display users. */
#define COLOR_BLACK   DISPLAY_COLOR_BLACK
#define COLOR_WHITE   DISPLAY_COLOR_WHITE
#define COLOR_RED     DISPLAY_COLOR_RED
#define COLOR_GREEN   DISPLAY_COLOR_GREEN
#define COLOR_BLUE    DISPLAY_COLOR_BLUE
#define COLOR_CYAN    DISPLAY_COLOR_CYAN
#define COLOR_YELLOW  DISPLAY_COLOR_YELLOW
#define COLOR_GRAY    DISPLAY_COLOR_GRAY
int display_init(void); uint16_t *display_backbuffer(void); void display_fill_back(uint16_t color); void display_swap(void);
#endif
