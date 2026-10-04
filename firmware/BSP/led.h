#ifndef __led_h__
#define __led_h__

#include "STC8H.H"

/* 状态灯是三态驱动（硬件文档 4.5 节）：
     低电平(0)  -> 绿色 LED 亮
     高电平(1)  -> 红色 LED 亮
     高阻(输入) -> 两个都灭
   TF_LED  = P1.6  (LED7 绿 / LED5 红)
   GPS_LED = P1.7  (LED6 绿 / LED4 红)
   另外 LED1/LED2/LED3 是电源指示灯，硬件常亮，软件管不了   */

#define LED_OFF 0 /* 高阻 -> 灭 */
#define LED_GRN 1 /* 低   -> 绿 */
#define LED_RED 2 /* 高   -> 红 */

void LED_INIT(void);

void LED_TF(unsigned char mode);  /* 0=灭 1=绿 2=红 */
void LED_GPS(unsigned char mode); /* 0=灭 1=绿 2=红 */

#endif
