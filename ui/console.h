#ifndef UI_CONSOLE_H
#define UI_CONSOLE_H
#include <stdint.h>
void console_init(uint16_t fg, uint16_t bg, unsigned int scale);
void console_clear(void);
void console_set_color(uint16_t fg);
void console_putln_highlight(const char *s, uint16_t fg, uint16_t bg);
void console_highlight_block_begin(uint16_t color);
void console_highlight_block_end(void);
void console_set_cursor(unsigned int column, unsigned int row);
void console_puts(const char *s);
void console_putln(const char *s);
void console_status(const char *label, const char *status, uint16_t status_color);
void console_present(void);
void console_present_swap_only(void);
#endif
