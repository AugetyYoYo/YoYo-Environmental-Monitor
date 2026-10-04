#include "bsp_ui.h"
#include "oled.h"

/* 电量竖条的几何：每条宽 5 像素、间隔 2 像素，高 2 页（16 像素） */
#define UI_BAR_W 5
#define UI_BAR_GAP 2
#define UI_BAR_H 2

/* \xCA\375字格式化的输出缓冲。
   放 xdata 且做成模块内静态：如果让调用方传缓冲进来，每个调用点都要
   在自己函\xCA\375里留 8 字节 DATA —— STC8H 的 DATA 只有 128 字节，很金贵。
   fmt10 和 fmtu 不会同时用，共用一个缓冲没问题。 */
static char xdata g_fmtbuf[8];

void bsp_ui_clear(void) {
    OLED_CLEAR();
}

void bsp_ui_line(unsigned char page, char *s) {
    static char xdata tmp[26];
    unsigned char w;
    unsigned char i;

    i = 0;
    while ((s[i] != 0) && (i < 25)) {
        tmp[i] = s[i];
        i = (unsigned char)(i + 1);
    }
    tmp[i] = 0;

    OLED_SHOW_STR(page, 0, (unsigned char *)tmp);

    /* 算这行已经占了多少"字符位"：ASCII 算 1，汉字（字节 >= 0x80）算 2 */
    w = 0;
    i = 0;
    while (tmp[i] != 0 && w < UI_LINE_CHARS)
    {
        if ((unsigned char)tmp[i] >= 0x80)
        {
            w = (unsigned char)(w + 2);
            i = (unsigned char)(i + 2);
        } else
        {
            w = (unsigned char)(w + 1);
            i = (unsigned char)(i + 1);
        }
    }

    /* 右边补空格，擦掉上一次显示留下的字符 */
    while (w < UI_LINE_CHARS)
    {
        OLED_SHOW_ASCII(page, (unsigned char)(w * 8), ' ');
        w = (unsigned char)(w + 1);
    }
}

void bsp_ui_bars(unsigned char page, unsigned char col, unsigned char bars, unsigned char blink) {
    unsigned char i;
    unsigned char c;

    if (blink)
    {
        return; /* 闪烁相位的暗半周：整条不画 */
    }

    for (i = 0; i < 4; i++)
    {
        c = (unsigned char)(col + i * (UI_BAR_W + UI_BAR_GAP));
        if (i < bars)
        {
            OLED_FILL_RECT(page, c, UI_BAR_W, UI_BAR_H, 0xFF); /* 点亮：填满 */
        } else
        {
            OLED_FILL_RECT(page, c, UI_BAR_W, UI_BAR_H, 0x81); /* 不亮：只留上下边 */
        }
    }
}

/* 把倒着写进去的\xCA\375字反转\xB9\375来 */
static void ui_reverse(char *buf, unsigned char n) {
    unsigned char i;
    char t;

    for (i = 0; i < (unsigned char)(n / 2); i++)
    {
        t = buf[i];
        buf[i] = buf[n - 1 - i];
        buf[n - 1 - i] = t;
    }
    buf[n] = 0;
}

/* 定点\xCA\375（×10）格式化成字符串：272 -> "27.2"，-45 -> "-4.5" */
char *bsp_ui_fmt10(int v) {
    unsigned char n;
    unsigned int a;

    n = 0;
    if (v < 0)
    {
        a = (unsigned int)(-v);
        g_fmtbuf[n] = (char)('0' + (a % 10U)); /* 小\xCA\375位 */
        n++;
        g_fmtbuf[n] = '.';
        n++;
        a = a / 10U;
        do
        {
            g_fmtbuf[n] = (char)('0' + (a % 10U));
            n++;
            a = a / 10U;
        } while (a != 0U);
        g_fmtbuf[n] = '-';
        n++;
    } else
    {
        a = (unsigned int)v;
        g_fmtbuf[n] = (char)('0' + (a % 10U));
        n++;
        g_fmtbuf[n] = '.';
        n++;
        a = a / 10U;
        do
        {
            g_fmtbuf[n] = (char)('0' + (a % 10U));
            n++;
            a = a / 10U;
        } while (a != 0U);
    }

    ui_reverse(g_fmtbuf, n);
    return g_fmtbuf;
}

/* 无符号整\xCA\375格式化成字符串：1234 -> "1234" */
char *bsp_ui_fmtu(unsigned int v) {
    unsigned char n;

    n = 0;
    do
    {
        g_fmtbuf[n] = (char)('0' + (v % 10U));
        n++;
        v = v / 10U;
    } while (v != 0U);

    ui_reverse(g_fmtbuf, n);
    return g_fmtbuf;
}
