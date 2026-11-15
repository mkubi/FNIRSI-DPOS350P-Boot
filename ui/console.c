#include "console.h"
#include "font8x8.h"
#include "display.h"
static unsigned int cx,cy,sc=2U; static uint16_t fg=0xFFFFU,bg=0x0000U;
static int highlight_block; static uint16_t highlight_color;
#define CONSOLE_X_ORIGIN 16U
#define CONSOLE_Y_ORIGIN 16U
#define CONSOLE_MENU_X CONSOLE_X_ORIGIN
#define CONSOLE_MENU_WIDTH 512U
static void fill(uint16_t c){uint16_t*p=display_backbuffer();unsigned int i;for(i=0;i<DISPLAY_WIDTH*DISPLAY_HEIGHT;i++)p[i]=c;}
static void ch(unsigned int x0,unsigned int y0,char c,uint16_t color){const uint8_t*g=font8x8_get(c);uint16_t*p=display_backbuffer();unsigned int x,y,sx,sy;for(y=0;y<8;y++)for(x=0;x<8;x++)if(g[y]&(0x80U>>x))for(sy=0;sy<sc;sy++)for(sx=0;sx<sc;sx++){unsigned int px=x0+x*sc+sx,py=y0+y*sc+sy;if(px<DISPLAY_WIDTH&&py<DISPLAY_HEIGHT)p[py*DISPLAY_WIDTH+px]=color;}}
static void nl(void){cx=CONSOLE_X_ORIGIN;cy+=10U*sc;}
void console_init(uint16_t f,uint16_t b,unsigned int scale){fg=f;bg=b;sc=scale?scale:1U;console_clear();}
void console_clear(void){fill(bg);cx=CONSOLE_X_ORIGIN;cy=CONSOLE_Y_ORIGIN;}
void console_set_color(uint16_t c){fg=c;}
static void fill_row(uint16_t c){uint16_t*p=display_backbuffer();unsigned int x,y;unsigned int h=8U*sc;if(cy>=DISPLAY_HEIGHT)return;if(cy+h>DISPLAY_HEIGHT)h=DISPLAY_HEIGHT-cy;for(y=0;y<h;y++)for(x=0;x<DISPLAY_WIDTH;x++)p[(cy+y)*DISPLAY_WIDTH+x]=c;}
static void fill_menu_row(uint16_t color){uint16_t*p=display_backbuffer();unsigned int x,y,h=8U*sc,x1=CONSOLE_MENU_X+CONSOLE_MENU_WIDTH;if(cy>=DISPLAY_HEIGHT)return;if(cy+h>DISPLAY_HEIGHT)h=DISPLAY_HEIGHT-cy;if(x1>DISPLAY_WIDTH)x1=DISPLAY_WIDTH;for(y=0;y<h;y++)for(x=CONSOLE_MENU_X;x<x1;x++)p[(cy+y)*DISPLAY_WIDTH+x]=color;}
void console_highlight_block_begin(uint16_t color){highlight_block=1;highlight_color=color;}
void console_highlight_block_end(void){highlight_block=0;}
void console_putln_highlight(const char*s,uint16_t f,uint16_t b){uint16_t old=fg;fill_row(b);fg=f;console_putln(s);fg=old;}
void console_set_cursor(unsigned int column,unsigned int row)
{
    unsigned int char_w=8U*sc,char_h=8U*sc;
    unsigned int line_h=10U*sc;
    /* column is an absolute screen character column; row is a console row. */
    unsigned int x=column*char_w;
    unsigned int y=CONSOLE_Y_ORIGIN+row*line_h;
    if(x+char_w>DISPLAY_WIDTH)x=DISPLAY_WIDTH-char_w;
    if(y+char_h>DISPLAY_HEIGHT)y=DISPLAY_HEIGHT-char_h;
    cx=x;cy=y;
}
void console_puts(const char*s){int row_filled=0;while(*s){if(*s=='\n'){nl();row_filled=0;s++;continue;}if(cx+8U*sc>=DISPLAY_WIDTH){nl();row_filled=0;}if(highlight_block&&!row_filled){fill_menu_row(highlight_color);row_filled=1;}ch(cx,cy,*s++,fg);cx+=8U*sc;}}
void console_putln(const char*s){console_puts(s);nl();}
void console_status(const char*label,const char*status,uint16_t c){uint16_t old=fg;console_puts(label);console_puts(" ");fg=c;console_putln(status);fg=old;}
void console_present(void)
{
    uint16_t *presented = display_backbuffer();
    uint16_t *next;
    unsigned int i;

    display_swap();

    /* display_swap() makes the previously displayed front buffer the new
     * back buffer. Mirror the just-presented image into it so subsequent
     * console writes are incremental instead of starting from an old frame. */
    next = display_backbuffer();
    for (i = 0U; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i)
        next[i] = presented[i];
}

void console_present_swap_only(void)
{
    /*
     * Full-redraw fast path: the caller will clear/redraw the next backbuffer,
     * so mirroring the complete 1024x600 frame is unnecessary.
     */
    display_swap();
}
