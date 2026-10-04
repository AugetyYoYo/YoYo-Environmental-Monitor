#ifndef __font_h__
#define __font_h__

#include "STC8H.H"

/* GT20L16S1Y 标准字库芯片（SPI）
   ROM_CS = P5.3（板子上的 OLED_ROM_CS）
   SPI 与 BMP581 / TF / OLED 共用：MOSI=P1.3, MISO=P1.4, CLK=P1.5
   命令：0x03 + 24 位地址 -> 连续读点阵字节                       */

sbit ROM_CS = P5 ^ 3;

void FONT_INIT(void);

/* 从字库读 n 个字节 */
void FONT_READ(unsigned long addr, unsigned char *buf, unsigned char n);

/* 8x16 ASCII 字符地址 */
unsigned long FONT_ADDR_ASCII(unsigned char c);

/* 15x16 汉字地址（GB2312 内码：msb 高字节, lsb 低字节） */
unsigned long FONT_ADDR_CN(unsigned char msb, unsigned char lsb);

#endif
