#include "console.h"
#include "font8x8.h"
#include "display.h"
static unsigned int cx,cy,sc=2U; static uint16_t fg=0xFFFFU,bg=0x0000U;
static void fill(uint16_t c){uint16_t*p=display_backbuffer();unsigned int i;for(i=0;i<DISPLAY_WIDTH*DISPLAY_HEIGHT;i++)p[i]=c;}
static void ch(unsigned int x0,unsigned int y0,char c,uint16_t color){const uint8_t*g=font8x8_get(c);uint16_t*p=display_backbuffer();unsigned int x,y,sx,sy;for(y=0;y<8;y++)for(x=0;x<8;x++)if(g[y]&(0x80U>>x))for(sy=0;sy<sc;sy++)for(sx=0;sx<sc;sx++){unsigned int px=x0+x*sc+sx,py=y0+y*sc+sy;if(px<DISPLAY_WIDTH&&py<DISPLAY_HEIGHT)p[py*DISPLAY_WIDTH+px]=color;}}
static void nl(void){cx=16U;cy+=10U*sc;}
void console_init(uint16_t f,uint16_t b,unsigned int scale){fg=f;bg=b;sc=scale?scale:1U;console_clear();}
void console_clear(void){fill(bg);cx=16U;cy=16U;}
void console_set_color(uint16_t c){fg=c;}
void console_set_cursor(unsigned int column,unsigned int row)
{
    unsigned int char_w=8U*sc,char_h=8U*sc;
    unsigned int x=column*char_w,y=row*char_h;
    if(x+char_w>DISPLAY_WIDTH)x=DISPLAY_WIDTH-char_w;
    if(y+char_h>DISPLAY_HEIGHT)y=DISPLAY_HEIGHT-char_h;
    cx=x;cy=y;
}
void console_puts(const char*s){while(*s){if(*s=='\n'){nl();s++;continue;}if(cx+8U*sc>=DISPLAY_WIDTH)nl();ch(cx,cy,*s++,fg);cx+=8U*sc;}}
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
