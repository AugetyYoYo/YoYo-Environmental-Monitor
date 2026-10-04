#ifndef __oled_h__
#define __oled_h__

#include "STC8H.H"

/* SSD1306 128x64 SPI OLED
   引脚（硬件文档 5 节 P1 排针）：
     OLED_CS     = P0.5
     OLED_DC     = P0.6   （高=数据，低=命令）
     OLED_ROM_CS = P5.3   （字库芯片片选，不用时拉高）
   SPI 与 BMP581 / TF 卡共用：MOSI=P1.3, MISO=P1.4, CLK=P1.5      */

sbit OLED_CS = P0 ^ 5;
sbit OLED_DC = P0 ^ 6;
sbit OLED_ROM_CS = P5 ^ 3;

void OLED_INIT(void);
void OLED_CLEAR(void);
void OLED_FILL(unsigned char pattern);
void OLED_FILL_RECT(unsigned char page, unsigned char col, unsigned char w,
                    unsigned char h, unsigned char pattern);
void OLED_TEST(void); /* 依次显示：全亮 -> 全灭 -> 棋盘 -> 边框 */
void OLED_SHOW_DIGITS(unsigned char n);
void OLED_SHOW_ASCII(unsigned char page, unsigned char col, unsigned char c);
void OLED_SHOW_CN(unsigned char page, unsigned char col, unsigned char msb, unsigned char lsb);
void OLED_SHOW_STR(unsigned char page, unsigned char col, unsigned char *s);
void OLED_SHOW_U16(unsigned char page, unsigned char col, unsigned int v); /* 显示 0~n 的数字（8x8 小字） */

#endif
