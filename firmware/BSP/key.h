#ifndef __key_h__
#define __key_h__

#include "STC8H.H"

/* 三个按键，按下接地（低电平有效）—— 硬件文档 4.5 节
   KEY_MODE = P2.7    KEY_UP = P4.6    KEY_DN = P0.0   */
sbit KEY_MODE = P2 ^ 7;
sbit KEY_UP = P4 ^ 6;
sbit KEY_DN = P0 ^ 0;

#define KEY_NONE 0
#define KEY_MODE_K 1
#define KEY_UP_K 2
#define KEY_DN_K 3

void KEY_INIT(void);

/* 扫描按键：有键按下返回 KEY_xxx_K（只在按下那一刻返回一次）
   松开或无按键返回 KEY_NONE，内部消抖阻塞约 20ms              */
unsigned char KEY_SCAN(void);

#endif
