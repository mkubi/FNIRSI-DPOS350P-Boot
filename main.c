#include <stdint.h>
#include "display.h"
#include "console.h"
#include "spi_nor_ro.h"

#define BLACK  0x0000U
#define WHITE  0xFFFFU
#define GREEN  0x07E0U
#define RED    0xF800U
#define YELLOW 0xFFE0U
#define CYAN   0x07FFU

#define EXE_HEADER_OFFSET  0x000A8000U
#define EXE_HEADER_SIZE    0x00000020U
#define EXE_TOTAL_SIZE     0x00095800U
#define EXE_PAYLOAD_OFFSET (EXE_HEADER_OFFSET + EXE_HEADER_SIZE)
#define EXE_PAYLOAD_SIZE   (EXE_TOTAL_SIZE - EXE_HEADER_SIZE)
#define EXE_LOAD_ADDRESS   0x40000000U
#define EXE_LOAD_PTR       ((uint8_t *)EXE_LOAD_ADDRESS)

static char hexn(uint8_t v){return v<10U?(char)('0'+v):(char)('A'+v-10U);}
static void hexbyte(uint8_t v){char s[3]={hexn(v>>4),hexn(v&15U),0};console_puts(s);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int header_ok(const uint8_t *p){
 return p[4]=='e'&&p[5]=='G'&&p[6]=='O'&&p[7]=='N'&&
        p[8]=='.'&&p[9]=='E'&&p[10]=='X'&&p[11]=='E'&&
        le32(p+16)==EXE_TOTAL_SIZE;
}
static void header(void){
 console_set_color(CYAN);console_putln("FNIRSI DPOS350P BOOTLOADER");
 console_set_color(WHITE);console_putln("------------------------");
 console_status("DISPLAY ........", "OK", GREEN);
 console_status("FRAMEBUFFER .....", "OK", GREEN);
 console_status("FONT ............", "OK", GREEN);
}

__attribute__((noinline))
static void call_exe_blx(uint32_t entry)
{
    /* Match the original SPL handoff: BLX entry. If it returns, caller loops. */
    __asm__ volatile("blx %0" : : "r"(entry) : "lr", "memory");
}

int main(void)
{
 spi_nor_jedec_id_t id;
 uint8_t hdr[EXE_HEADER_SIZE];
 uint8_t verify[32];
 unsigned int i;
 int r,hr=-99,load=-99,vr=-99,match=0;

 display_init();console_init(WHITE,BLACK,2U);header();
 console_status("SPI NOR .........", "PROBING", YELLOW);
 console_status("EXE1 PAYLOAD ....", "WAIT", YELLOW);
 console_present();

 r=spi_nor_ro_init();
 if(!r)r=spi_nor_read_jedec_id(&id);
 if(!r)hr=spi_nor_read_chunked(EXE_HEADER_OFFSET,hdr,sizeof(hdr));
 if(!r&&!hr&&header_ok(hdr))load=spi_nor_read_chunked(EXE_PAYLOAD_OFFSET,EXE_LOAD_PTR,EXE_PAYLOAD_SIZE);
 if(!r&&!hr&&!load)vr=spi_nor_read_chunked(EXE_PAYLOAD_OFFSET,verify,sizeof(verify));
 if(!r&&!hr&&!load&&!vr){match=1;for(i=0U;i<32U;i++)if(verify[i]!=EXE_LOAD_PTR[i]){match=0;break;}}

 console_clear();header();
 if(!r){
  console_status("SPI NOR .........", "OK", GREEN);
  console_set_color(WHITE);console_puts("JEDEC ID ........ ");
  console_set_color(CYAN);hexbyte(id.manufacturer);console_puts(" ");hexbyte(id.memory_type);console_puts(" ");hexbyte(id.capacity);console_putln("");
 } else console_status("SPI NOR .........", "FAIL", RED);

 if(!r&&!hr&&!load&&header_ok(hdr)&&match){
  console_status("EXE1 PAYLOAD ....", "OK", GREEN);
  console_status("HEADER/MATCH ....", "OK", GREEN);
  console_set_color(WHITE);console_putln("NOR 018020 -> RAM 40000000");
  console_putln("SIZE 247E0, HEADER STRIPPED");
  console_status("BLX 40000000 ....", "NOW", YELLOW);
  console_present();
  call_exe_blx(EXE_LOAD_ADDRESS);
  for(;;){}
 }

 console_status("EXE1 PAYLOAD ....", "FAIL", RED);
 console_status("BLX .............", "BLOCKED", RED);
 console_present();
 for(;;){}
}
