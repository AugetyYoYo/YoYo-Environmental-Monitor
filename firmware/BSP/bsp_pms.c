#include "bsp_pms.h"
#include "uart.h"

/* UART3 接收环形缓冲：中断里写、主循环读 */
#define PMS_RB_SIZE 64
static unsigned char xdata g_pms_rb[PMS_RB_SIZE];
static volatile unsigned char g_pms_head; /* 中断写 */
static volatile unsigned char g_pms_tail; /* 主循环读 */

static unsigned char xdata g_frame[32];
static unsigned char g_fi; /* 帧内已收字节\xCA\375 */
static pms_info_t xdata g_pms;

/* UART3 接收中断：每来一个字节丢进环（满了丢新字节） */
void uart3_isr(void) interrupt UART3_VECTOR {
    unsigned char next;

    if (S3CON & 0x01) /* S3RI */
    {
        S3CON &= ~0x01;
        next = (unsigned char)(g_pms_head + 1);
        if (next >= PMS_RB_SIZE)
        {
            next = 0;
        }
        if (next != g_pms_tail)
        {
            g_pms_rb[g_pms_head] = S3BUF;
            g_pms_head = next;
        }
    }
}

static unsigned char pms_rb_get(unsigned char *c) {
    if (g_pms_head == g_pms_tail)
    {
        return 0;
    }
    *c = g_pms_rb[g_pms_tail];
    g_pms_tail = (unsigned char)(g_pms_tail + 1);
    if (g_pms_tail >= PMS_RB_SIZE)
    {
        g_pms_tail = 0;
    }
    return 1;
}

void bsp_pms_init(void) {
    unsigned char i;

    g_pms.ok = 0;
    g_pms.pm1_0 = 0;
    g_pms.pm2_5 = 0;
    g_pms.pm10 = 0;
    g_pms_head = 0;
    g_pms_tail = 0;
    g_fi = 0;
    for (i = 0; i < 32; i++)
    {
        g_frame[i] = 0;
    }

    UART3_INIT();
    IE2 |= 0x08; /* ES3 = 1：开 UART3 接收中断（EA 已在 TICK_INIT 打开） */
}

/* 从环里取字节拼帧；校验通\xB9\375就更新\xCA\375据 */
void bsp_pms_poll(void) {
    unsigned char c;
    unsigned char i;
    unsigned int sum;

    while (pms_rb_get(&c))
    {
        if (g_fi == 0)
        {
            if (c == 0x42)
            {
                g_frame[0] = c;
                g_fi = 1;
            }
            continue;
        }
        if (g_fi == 1)
        {
            if (c == 0x4D)
            {
                g_frame[1] = c;
                g_fi = 2;
            } else if (c == 0x42)
            {
                g_frame[0] = c;
                g_fi = 1;
            } else
            {
                g_fi = 0;
            }
            continue;
        }

        g_frame[g_fi] = c;
        g_fi++;
        if (g_fi >= 32)
        {
            g_fi = 0;
            sum = 0;
            for (i = 0; i < 30; i++)
            {
                sum += g_frame[i];
            }
            if ((g_frame[2] == 0x00) && (g_frame[3] == 0x1C) && ((((unsigned int)g_frame[30] << 8) | g_frame[31]) == sum))
            {
                g_pms.pm1_0 = ((unsigned int)g_frame[10] << 8) | g_frame[11];
                g_pms.pm2_5 = ((unsigned int)g_frame[12] << 8) | g_frame[13];
                g_pms.pm10 = ((unsigned int)g_frame[14] << 8) | g_frame[15];
                g_pms.ok = 1;
            }
        }
    }
}

pms_info_t *bsp_pms_get(void) {
    return &g_pms;
}
