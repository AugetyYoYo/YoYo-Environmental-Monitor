#include "oled.h"
#include "font.h"
#include "spi.h"
#include "tick.h"

/* ------------------------------------------------------------------
   SSD1306 128x64（SPI）
   初始化序列取自你之前验证\xB9\375的\xC7\375动（C51_project 里的 oled.c）
   ------------------------------------------------------------------ */

/* 8x8 \xCA\375字字模 0~9，每个 8 字节，每字节一列（LSB 在上） */
static unsigned char code DIGITS[10][8] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00, 0x00}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00, 0x00, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46, 0x00, 0x00, 0x00}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31, 0x00, 0x00, 0x00}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00, 0x00, 0x00}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00, 0x00}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00, 0x00, 0x00}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03, 0x00, 0x00, 0x00}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x00, 0x00}, /* 9 */
};

static void OLED_CMD(unsigned char c) {
  OLED_CS = 0;
  OLED_DC = 0;
  SPI_XFER(c);
  OLED_CS = 1;
}

static void OLED_DAT(unsigned char d) {
  OLED_CS = 0;
  OLED_DC = 1;
  SPI_XFER(d);
  OLED_CS = 1;
}

void OLED_INIT(void) {
  /* \xD2\375脚：CS / DC / ROM_CS 推挽输出 */
  P0M1 &= ~0x20;
  P0M0 |= 0x20; /* P0.5 OLED_CS */
  P0M1 &= ~0x40;
  P0M0 |= 0x40; /* P0.6 OLED_DC */
  P5M1 &= ~0x08;
  P5M0 |= 0x08; /* P5.3 OLED_ROM_CS */

  OLED_CS = 1;
  OLED_DC = 1;
  OLED_ROM_CS = 1; /* 不用字库，拉高 */

  OLED_CMD(0xAE); /* 关闭显示 */
  OLED_CMD(0x00);
  OLED_CMD(0x10);
  OLED_CMD(0x40);
  OLED_CMD(0x81); /* 对比度 */
  OLED_CMD(0xCF);
  OLED_CMD(0xA0); /* 左右翻转 */
  OLED_CMD(0xC0); /* COM 扫描方向 */
  OLED_CMD(0xA6); /* \xD5\375常显示 */
  OLED_CMD(0xA8); /* 多路复用比 */
  OLED_CMD(0x3F);
  OLED_CMD(0xD3);
  OLED_CMD(0x00);
  OLED_CMD(0xD5); /* 时钟分频 */
  OLED_CMD(0x80);
  OLED_CMD(0xD9); /* 预充电周期 */
  OLED_CMD(0xF1);
  OLED_CMD(0xDA); /* COM 硬件配置 */
  OLED_CMD(0x12);
  OLED_CMD(0xDB); /* VCOMH */
  OLED_CMD(0x40);
  OLED_CMD(0x20); /* 寻址模式 */
  OLED_CMD(0x02); /* 页寻址 */
  OLED_CMD(0x8D); /* 电荷泵 */
  OLED_CMD(0x14);
  OLED_CMD(0xA4);
  OLED_CMD(0xA6);

  OLED_CLEAR();
  OLED_CMD(0xAF); /* 开启显示 */
  delay_ms(50);
}

/* 画一个实心/带边框的矩形（给电量竖条用）
   page  起始页      col   起始列（像素）
   w     宽（像素）  h     高（页\xCA\375，1 页 = 8 像素）
   pattern 每一列填的图案：0xFF = 全填满，0x81 = 只画上下两条边 */
/* OLED_POS 定义在后面，这里先声明一下 */
static void OLED_POS(unsigned char page, unsigned char col);

void OLED_FILL_RECT(unsigned char page, unsigned char col, unsigned char w,
                    unsigned char h, unsigned char pattern) {
  unsigned char p;
  unsigned char i;

  for (p = 0; p < h; p++) {
    OLED_POS((unsigned char)(page + p), col);
    for (i = 0; i < w; i++) {
      OLED_DAT(pattern);
    }
  }
}
void OLED_CLEAR(void) {
  unsigned char p;
  unsigned char i;

  for (p = 0; p < 8; p++) {
    OLED_CMD((unsigned char)(0xB0 + p)); /* 页地址 */
    OLED_CMD(0x00);                      /* 列低 4 位 */
    OLED_CMD(0x10);                      /* 列高 4 位 */
    for (i = 0; i < 128; i++) {
      OLED_DAT(0x00);
    }
  }
}

void OLED_FILL(unsigned char pattern) {
  unsigned char p;
  unsigned char i;

  for (p = 0; p < 8; p++) {
    OLED_CMD((unsigned char)(0xB0 + p));
    OLED_CMD(0x00);
    OLED_CMD(0x10);
    for (i = 0; i < 128; i++) {
      OLED_DAT(pattern);
    }
  }
}

/* 依次显示 4 种图案，每种 800ms */
void OLED_TEST(void) {
  unsigned char p;
  unsigned char i;

  /* ① 全亮 */
  OLED_FILL(0xFF);
  delay_ms(800);

  /* ② 全灭 */
  OLED_FILL(0x00);
  delay_ms(800);

  /* ③ 棋盘格 */
  for (p = 0; p < 8; p++) {
    OLED_CMD((unsigned char)(0xB0 + p));
    OLED_CMD(0x00);
    OLED_CMD(0x10);
    for (i = 0; i < 128; i++) {
      OLED_DAT((unsigned char)(((i >> 2) ^ p) & 1 ? 0x0F : 0xF0));
    }
  }
  delay_ms(800);

  /* ④ 边框 */
  OLED_FILL(0x00);
  for (p = 0; p < 8; p++) {
    OLED_CMD((unsigned char)(0xB0 + p));
    OLED_CMD(0x00);
    OLED_CMD(0x10);
    for (i = 0; i < 128; i++) {
      if (p == 0 || p == 7 || i == 0 || i == 127) {
        OLED_DAT(0xFF);
      } else {
        OLED_DAT(0x81);
      }
    }
  }
  delay_ms(800);
}

/* 在屏幕左上角显示 0~n 的\xCA\375字（8x8） */
void OLED_SHOW_DIGITS(unsigned char n) {
  unsigned char d;
  unsigned char i;

  OLED_CLEAR();
  OLED_CMD(0xB0);
  OLED_CMD(0x00);
  OLED_CMD(0x10);

  for (d = 0; d <= n && d <= 9; d++) {
    for (i = 0; i < 8; i++) {
      OLED_DAT(DIGITS[d][i]);
    }
    OLED_DAT(0x00); /* 间隔 */
  }
}

/* ---------------- 字库芯片相关（GT20L16S1Y） ---------------- */

static void OLED_POS(unsigned char page, unsigned char col) {
  OLED_CMD((unsigned char)(0xB0 + page));
  OLED_CMD((unsigned char)(col & 0x0F));
  OLED_CMD((unsigned char)(0x10 | ((col >> 4) & 0x0F)));
}

/* 显示一个 8x16 ASCII 字符（从字库读） */
void OLED_SHOW_ASCII(unsigned char page, unsigned char col, unsigned char c) {
  static unsigned char xdata g[16];
  unsigned char i;

  FONT_READ(FONT_ADDR_ASCII(c), g, 16);

  OLED_POS(page, col);
  for (i = 0; i < 8; i++) {
    OLED_DAT(g[i]);
  }
  OLED_POS((unsigned char)(page + 1), col);
  for (i = 8; i < 16; i++) {
    OLED_DAT(g[i]);
  }
}

/* 显示一个 15x16 汉字（从字库读），msb/lsb 是 GB2312 内码 */
void OLED_SHOW_CN(unsigned char page, unsigned char col, unsigned char msb, unsigned char lsb) {
  static unsigned char xdata g[32];
  unsigned char i;
  unsigned long a;

  a = FONT_ADDR_CN(msb, lsb);
  if (a == 0) {
    return;
  }
  FONT_READ(a, g, 32);

  OLED_POS(page, col);
  for (i = 0; i < 16; i++) {
    OLED_DAT(g[i]);
  }
  OLED_POS((unsigned char)(page + 1), col);
  for (i = 16; i < 32; i++) {
    OLED_DAT(g[i]);
  }
}

/* 显示字符串（ASCII 与汉字混排），从 (page,col) 开始 */
void OLED_SHOW_STR(unsigned char page, unsigned char col, unsigned char *s) {
  while (*s != 0) {
    if (*s >= 0x80) { /* 汉字（GB2312 双字节） */
      if (col > 112) { break; } /* no-wrap */
      OLED_SHOW_CN(page, col, s[0], s[1]);
      s += 2;
      col += 16;
    } else {
      if (col > 120) { break; } /* no-wrap */
      OLED_SHOW_ASCII(page, col, *s);
      s += 1;
      col += 8;
    }
    if (col > 120) {
      break;
    }
  }
}

/* 显示一个无符号十进制\xCA\375 */
void OLED_SHOW_U16(unsigned char page, unsigned char col, unsigned int v) {
  static unsigned char xdata buf[5];
  unsigned char n;
  unsigned char i;

  if (v == 0) {
    OLED_SHOW_ASCII(page, col, '0');
    return;
  }
  n = 0;
  while (v > 0) {
    buf[n] = (unsigned char)(v % 10) + '0';
    n++;
    v /= 10;
  }
  for (i = n; i > 0; i--) {
    OLED_SHOW_ASCII(page, col, buf[i - 1]);
    col += 8;
  }
}