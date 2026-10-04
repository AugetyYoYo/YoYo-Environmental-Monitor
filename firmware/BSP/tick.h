#ifndef __tick_h__
#define __tick_h__

#include "STC8H.H"

/* 开机以来的毫秒数（Timer0 每 1ms 加一） */
extern volatile unsigned int g_ms;

void TICK_INIT(void);

/* 阻塞延时，单位毫秒 */
void delay_ms(unsigned int ms);

#endif
