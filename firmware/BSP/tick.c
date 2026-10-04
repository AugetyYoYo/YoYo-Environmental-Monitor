#include "tick.h"
#include "Config.h"

volatile unsigned int g_ms = 0;

/* Timer0 做 1ms 中断
   1T 模式（AUXR.7 = T0x12 = 1），16 位
   重装值 = 65536 - MAIN_Fosc / 1000
   24MHz 时 = 65536 - 24000 = 0xA240                    */
#define T0_RELOAD (65536UL - (MAIN_Fosc / 1000UL))
#define T0_H ((unsigned char)(T0_RELOAD >> 8))
#define T0_L ((unsigned char)(T0_RELOAD))

void TICK_INIT(void) {
  AUXR |= 0x80; /* T0x12 = 1 : Timer0 用 Fosc（1T） */

  TMOD &= 0xF0;
  TMOD |= 0x01; /* Timer0，模式 1（16 位） */

  TH0 = T0_H;
  TL0 = T0_L;

  ET0 = 1; /* 允许 Timer0 中断 */
  TR0 = 1; /* 启动 Timer0 */
  EA = 1;  /* 开总中断 */
}

void timer0_isr(void) interrupt 1 {
  TH0 = T0_H;
  TL0 = T0_L;
  g_ms++;
}

void delay_ms(unsigned int ms) {
  unsigned int start;

  start = g_ms;
  while ((unsigned int)(g_ms - start) < ms) {
  }
}
