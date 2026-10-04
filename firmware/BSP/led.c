#include "led.h"

/* 把 P1.6 / P1.7 设成高阻输入（灯灭） */
void LED_INIT(void) {
  P1M1 |= 0x40;
  P1M0 &= ~0x40; /* P1.6 高阻 */
  P1M1 |= 0x80;
  P1M0 &= ~0x80; /* P1.7 高阻 */
}

/* P1.6 \xC8\375态控制 */
void LED_TF(unsigned char mode) {
  if (mode == LED_OFF) {
    P1M1 |= 0x40;
    P1M0 &= ~0x40; /* 高阻 */
  } else {
    P1M1 &= ~0x40;
    P1M0 |= 0x40; /* 推挽输出 */
    if (mode == LED_GRN) {
      P16 = 0; /* 低 -> 绿 */
    } else {
      P16 = 1; /* 高 -> 红 */
    }
  }
}

/* P1.7 \xC8\375态控制 */
void LED_GPS(unsigned char mode) {
  if (mode == LED_OFF) {
    P1M1 |= 0x80;
    P1M0 &= ~0x80; /* 高阻 */
  } else {
    P1M1 &= ~0x80;
    P1M0 |= 0x80; /* 推挽输出 */
    if (mode == LED_GRN) {
      P17 = 0; /* 低 -> 绿 */
    } else {
      P17 = 1; /* 高 -> 红 */
    }
  }
}
