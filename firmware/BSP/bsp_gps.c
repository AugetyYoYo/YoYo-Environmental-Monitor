#include "bsp_gps.h"
#include "bsp_rtc.h"
#include "uart.h"

/* NMEA 一行最长约 82 字符 */
#define GPS_LINE_MAX 100

/* UART2 接收环形缓冲（中断里写、主循环读） */
#define GPS_RB_SIZE 64

/* 平年的每月天数（闰年 2 月改 29�?*/
static const unsigned char code g_days_in_month[12] = {31, 28, 31, 30, 31, 30,
                                                       31, 31, 30, 31, 30, 31};

static gps_info_t xdata g_gps;
static unsigned char xdata g_line[GPS_LINE_MAX];
static unsigned char xdata g_f[12];
static unsigned char g_len;
static unsigned char g_sv_acc;

static unsigned char xdata g_rb[GPS_RB_SIZE];
static volatile unsigned char g_h;
static volatile unsigned char g_t;

/* UART2 接收中断：每来一个字节丢进环 */
void uart2_isr(void) interrupt UART2_VECTOR {
    unsigned char n;

    if (S2CON & 0x01) /* S2RI */
    {
        S2CON &= ~0x01;
        n = (unsigned char)(g_h + 1);
        if (n >= GPS_RB_SIZE)
        {
            n = 0;
        }
        if (n != g_t)
        {
            g_rb[g_h] = S2BUF;
            g_h = n;
        }
    }
    if (S2CON & 0x02) /* S2TI：本模块不发送，清了防止中断卡死 */
    {
        S2CON &= ~0x02;
    }
}

static unsigned char gps_rb_get(unsigned char *c) {
    if (g_h == g_t)
    {
        return 0;
    }
    *c = g_rb[g_t];
    g_t = (unsigned char)(g_t + 1);
    if (g_t >= GPS_RB_SIZE)
    {
        g_t = 0;
    }
    return 1;
}

/*==================================================================
  内部工具
==================================================================*/

static unsigned char gps_is_leap(unsigned int y) {
    if (y % 4U != 0U)
    {
        return 0;
    }
    if (y % 100U != 0U)
    {
        return 1;
    }
    return (unsigned char)((y % 400U == 0U) ? 1 : 0);
}

/* �?NMEA �?n 个字段（0 起）放到 out，返回长度，失败返回 0xFF */
static unsigned char gps_get_field(unsigned char n, unsigned char *out, unsigned char outlen) {
    unsigned char i;
    unsigned char f;
    unsigned char k;

    f = 0;
    i = 0;
    while (g_line[i] != 0 && g_line[i] != '*')
    {
        if (f == n)
        {
            break;
        }
        if (g_line[i] == ',')
        {
            f++;
        }
        i++;
    }
    if (f != n)
    {
        return 0xFF;
    }
    k = 0;
    while (g_line[i] != 0 && g_line[i] != ',' && g_line[i] != '*' &&
           k < (unsigned char)(outlen - 1))
    {
        out[k] = g_line[i];
        k++;
        i++;
    }
    out[k] = 0;
    return k;
}

static unsigned int gps_get_num(unsigned char *out, unsigned char n) {
    unsigned int v;
    unsigned char i;

    v = 0;
    for (i = 0; i < n; i++)
    {
        if (out[i] < '0' || out[i] > '9')
        {
            break;
        }
        v = v * 10U + (unsigned int)(out[i] - '0');
    }
    return v;
}

/* 解析 "ddmm.mmmm"/"dddmm.mmmm" -> 整数�?+ 小数×10000 */
static void gps_parse_coord(unsigned char *f, unsigned char ddigits, unsigned int *deg,
                            unsigned int *f4) {
    unsigned int d;
    unsigned int mm;
    unsigned long min4;
    unsigned char i;
    unsigned char k;
    unsigned char c;

    d = 0;
    i = 0;
    while (i < ddigits)
    {
        d = d * 10U + (unsigned int)(f[i] - '0');
        i++;
    }
    mm = (unsigned int)(f[i] - '0') * 10U + (unsigned int)(f[i + 1] - '0');
    i = (unsigned char)(i + 2);
    if (f[i] == '.')
    {
        i++;
    }
    min4 = 0;
    for (k = 0; k < 4; k++)
    {
        c = f[i + k];
        if (c >= '0' && c <= '9')
        {
            min4 = min4 * 10UL + (unsigned long)(c - '0');
        } else
        {
            min4 = min4 * 10UL;
        }
    }
    min4 = (unsigned long)mm * 10000UL + min4;
    *deg = d;
    *f4 = (unsigned int)(min4 / 60UL);
}

static unsigned char gps_eq(char a, char b) {
    return (unsigned char)(a == b);
}

/* 解析一�?NMEA */
static void gps_parse_line(unsigned char *line) {
    unsigned int yr;
    unsigned char j;

    if (line[0] != '$')
    {
        return;
    }
    g_gps.lines++;

    /* $GPTXT 里带天线状�?*/
    if (line[3] == 'T' && line[4] == 'X' && line[5] == 'T')
    {
        j = 0;
        while (line[j] != 0)
        {
            if (line[j] == 'A' && line[j + 1] == 'N' && line[j + 2] == 'T' && line[j + 3] == 'E' &&
                line[j + 4] == 'N' && line[j + 5] == 'N' && line[j + 6] == 'A')
            {
                if (line[j + 8] == 'O' && line[j + 9] == 'K')
                {
                    g_gps.ant = 1;
                } else if (line[j + 8] == 'O' && line[j + 9] == 'P')
                {
                    g_gps.ant = 2;
                } else if (line[j + 8] == 'S')
                { g_gps.ant = 3; }
                break;
            }
            j++;
        }
        return;
    }

    /* $GxRMC */
    if (gps_eq(line[3], 'R') && gps_eq(line[4], 'M') && gps_eq(line[5], 'C'))
    {
        g_gps.sats_view = g_sv_acc;
        g_sv_acc = 0;

        /* �?经度（字�?3~6�?*/
        if (gps_get_field(3, g_f, 12) >= 4)
        {
            gps_parse_coord(g_f, 2, &g_gps.lat_d, &g_gps.lat_f4);
        }
        if (gps_get_field(4, g_f, 2) == 1)
        {
            g_gps.lat_ns = g_f[0];
        }
        if (gps_get_field(5, g_f, 12) >= 5)
        {
            gps_parse_coord(g_f, 3, &g_gps.lon_d, &g_gps.lon_f4);
        }
        if (gps_get_field(6, g_f, 2) == 1)
        {
            g_gps.lon_ew = g_f[0];
        }

        if (gps_get_field(2, g_f, 12) == 1 && g_f[0] == 'A')
        {
            g_gps.fix = 1;
            if (gps_get_field(9, g_f, 12) == 6)
            {
                g_gps.day = (unsigned char)gps_get_num(g_f, 2);
                g_gps.month = (unsigned char)gps_get_num(&g_f[2], 2);
                g_gps.year = (unsigned char)gps_get_num(&g_f[4], 2);
                if (gps_get_field(1, g_f, 12) >= 6)
                {
                    g_gps.hour = (unsigned char)gps_get_num(g_f, 2);
                    g_gps.minute = (unsigned char)gps_get_num(&g_f[2], 2);
                    g_gps.second = (unsigned char)gps_get_num(&g_f[4], 2);
                    g_gps.valid = 1;
                }
            }
        }
    }
    /* $GxGGA */
    else if (gps_eq(line[3], 'G') && gps_eq(line[4], 'G') && gps_eq(line[5], 'A'))
    {
        if (gps_get_field(6, g_f, 12) == 1)
        {
            if (g_f[0] >= '1' && g_f[0] <= '9')
            {
                g_gps.fix = 1;
            }
        }
        if (gps_get_field(7, g_f, 12) >= 1)
        {
            g_gps.sats_used = (unsigned char)gps_get_num(g_f, 2);
        }
    }
    /* $GxGSV */
    else if (gps_eq(line[3], 'G') && gps_eq(line[4], 'S') && gps_eq(line[5], 'V'))
    {
        if (gps_get_field(2, g_f, 12) == 1 && g_f[0] == '1')
        {
            if (gps_get_field(3, g_f, 12) >= 1)
            {
                g_sv_acc = (unsigned char)(g_sv_acc + (unsigned char)gps_get_num(g_f, 2));
            }
        }
    }
    /* $GxZDA */
    else if (gps_eq(line[3], 'Z') && gps_eq(line[4], 'D') && gps_eq(line[5], 'A'))
    {
        if (gps_get_field(4, g_f, 12) == 4)
        {
            yr = gps_get_num(g_f, 4);
            if (yr >= 2000U && yr <= 2099U)
            {
                g_gps.year = (unsigned char)(yr - 2000U);
                if (gps_get_field(2, g_f, 12) == 2)
                {
                    g_gps.day = (unsigned char)gps_get_num(g_f, 2);
                }
                if (gps_get_field(3, g_f, 12) == 2)
                {
                    g_gps.month = (unsigned char)gps_get_num(g_f, 2);
                }
                if (gps_get_field(1, g_f, 12) >= 6)
                {
                    g_gps.hour = (unsigned char)gps_get_num(g_f, 2);
                    g_gps.minute = (unsigned char)gps_get_num(&g_f[2], 2);
                    g_gps.second = (unsigned char)gps_get_num(&g_f[4], 2);
                    g_gps.valid = 1;
                }
            }
        }
    }
}

/*==================================================================
  对外接口
==================================================================*/

void bsp_gps_init(void) {
    unsigned char i;

    g_gps.fix = 0;
    g_gps.sats_used = 0;
    g_gps.sats_view = 0;
    g_gps.year = 0;
    g_gps.month = 0;
    g_gps.day = 0;
    g_gps.hour = 0;
    g_gps.minute = 0;
    g_gps.second = 0;
    g_gps.valid = 0;
    g_gps.rtc_synced = 0;
    g_gps.lines = 0;
    g_gps.ant = 0;
    g_gps.lat_ns = 0;
    g_gps.lon_ew = 0;
    g_gps.lat_d = 0;
    g_gps.lat_f4 = 0;
    g_gps.lon_d = 0;
    g_gps.lon_f4 = 0;

    g_len = 0;
    g_sv_acc = 0;
    g_h = 0;
    g_t = 0;
    for (i = 0; i < GPS_LINE_MAX; i++)
    {
        g_line[i] = 0;
    }

    UART2_INIT();
    UART2_SET_BAUD(115200UL);
    IE2 |= 0x01; /* ES2 = 1：开 UART2 接收中断 */
}

void bsp_gps_poll(void) {
    unsigned char c;

    while (gps_rb_get(&c))
    {
        if (c == '\n')
        {
            g_line[g_len] = 0;
            gps_parse_line(g_line);
            g_len = 0;
        } else if (c != '\r')
        {
            if (g_len < (unsigned char)(GPS_LINE_MAX - 1))
            {
                g_line[g_len] = c;
                g_len++;
            } else
            {
                g_len = 0;
            }
        }
    }
}

gps_info_t *bsp_gps_get(void) {
    return &g_gps;
}

unsigned char bsp_gps_sync_rtc(void) {
    rtc_time_t t;
    unsigned int y;
    unsigned char dim;

    if (g_gps.valid == 0 || g_gps.rtc_synced != 0)
    {
        return 0;
    }
    if (g_gps.month < 1 || g_gps.month > 12 || g_gps.day < 1 || g_gps.day > 31 || g_gps.hour > 23)
    {
        return 0;
    }

    y = (unsigned int)(2000U + g_gps.year);
    t.month = g_gps.month;
    t.day = g_gps.day;
    t.hour = (unsigned char)(g_gps.hour + 8);
    t.minute = g_gps.minute;
    t.second = g_gps.second;

    if (t.hour >= 24)
    {
        t.hour = (unsigned char)(t.hour - 24);
        t.day = (unsigned char)(t.day + 1);

        dim = g_days_in_month[t.month - 1];
        if (t.month == 2 && gps_is_leap(y))
        {
            dim = 29;
        }
        if (t.day > dim)
        {
            t.day = 1;
            t.month = (unsigned char)(t.month + 1);
            if (t.month > 12)
            {
                t.month = 1;
                y = y + 1U;
            }
        }
    }

    t.year = (unsigned char)(y % 100U);
    t.weekday = bsp_rtc_weekday(t.year, t.month, t.day);
    t.valid = 1;

    bsp_rtc_write(&t);
    g_gps.rtc_synced = 1;
    return 1;
}
