#include "font.h"
#include "spi.h"

void FONT_INIT(void) {
  P5M1 &= ~0x08;
  P5M0 |= 0x08; /* P5.3 鎺ㄦ尳 */
  ROM_CS = 1;
}

/* 位翻转 SPI（照老师的时序：SCLK 拉低时采样 MISO）
   用位翻转是因为字库芯片对采样时刻敏感，硬件 SPI 的上\xC9\375沿采样读不对 */
sbit F_SCK = P1^5;
sbit F_MOSI = P1^3;
sbit F_MISO = P1^4;

static void rom_out(unsigned char d) {
  unsigned char i;

  for (i = 0; i < 8; i++) {
    F_SCK = 0;
    if (d & 0x80) {
      F_MOSI = 1;
    } else {
      F_MOSI = 0;
    }
    d <<= 1;
    F_SCK = 1;
  }
}

static unsigned char rom_in(void) {
  unsigned char i;
  unsigned char v;

  v = 0;
  for (i = 0; i < 8; i++) {
    F_SCK = 0;
    v <<= 1;
    if (F_MISO) {
      v |= 0x01;
    }
    F_SCK = 1;
  }
  return v;
}

void FONT_READ(unsigned long addr, unsigned char *buf, unsigned char n) {
  unsigned char i;

  SPCTL &= ~SPEN; /* 先关掉硬件 SPI，避免抢\xD2\375脚 */

  P1M1 &= ~0x08;
  P1M0 |= 0x08; /* P1.3 MOSI 推挽 */
  P1M1 &= ~0x20;
  P1M0 |= 0x20; /* P1.5 SCK 推挽 */
  P1M1 &= ~0x10;
  P1M0 &= ~0x10; /* P1.4 MISO 输入 */

  F_SCK = 0;
  F_MOSI = 1;

  ROM_CS = 0;
  rom_out(0x03);
  rom_out((unsigned char)(addr >> 16));
  rom_out((unsigned char)(addr >> 8));
  rom_out((unsigned char)addr);
  for (i = 0; i < n; i++) {
    buf[i] = rom_in();
  }
  ROM_CS = 1;
  F_SCK = 0; /* 恢复成模式 0 的空闲电平（低） */

  SPCTL |= SPEN; /* 恢复硬件 SPI */
}
unsigned long FONT_ADDR_ASCII(unsigned char c) {
  /* 8x16 鐐?ASCII锛欱aseAdd = 0x3B7C0锛屾瘡涓?16 瀛楄妭 */
  if (c < 0x20 || c > 0x7E) {
    return 0x3B7C0UL;
  }
  return (unsigned long)(c - 0x20) * 16UL + 0x3B7C0UL;
}

unsigned long FONT_ADDR_CN(unsigned char msb, unsigned char lsb) {
  /* 15x16 鐐?GB2312 姹夊瓧锛屾瘡涓?32 瀛楄妭锛堣鎵嬪唽 6.3.1.1锛?*/
  if (msb == 0xA9 && lsb >= 0xA1) {
    return (unsigned long)(282 + (lsb - 0xA1)) * 32UL;
  }
  if (msb >= 0xA1 && msb <= 0xA3 && lsb >= 0xA1) {
    return (unsigned long)((unsigned long)(msb - 0xA1) * 94UL + (lsb - 0xA1)) *
           32UL;
  }
  if (msb >= 0xB0 && msb <= 0xF7 && lsb >= 0xA1) {
    return (unsigned long)((unsigned long)(msb - 0xB0) * 94UL + (lsb - 0xA1) +
                           846UL) *
           32UL;
  }
  return 0;
}
