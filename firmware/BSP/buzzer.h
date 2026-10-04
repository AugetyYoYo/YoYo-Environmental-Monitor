#ifndef __buzzer_h__
#define __buzzer_h__

#include "STC8H.H"

/* 蜂鸣器：P3.4 经 R15(1k) 驱动 Q1(S8050)，5V 供电
   型号 MLT-8530（无源），靠方波驱动                    */
sbit BUZZER = P3 ^ 4;

void BUZZER_INIT(void);
void BUZZER_ON(void);
void BUZZER_OFF(void);

/* 响 ms 毫秒（输出方波） */
void BUZZER_BEEP(unsigned int ms);

#endif
