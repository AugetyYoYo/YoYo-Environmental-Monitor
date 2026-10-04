#include "uart.h"
#include "Config.h"

/*==============================================================
  波特率计算
  ------------------------------------------------------------
  UART1（用 Timer1，1T 模式 + SMOD=1）：
     重装值 = 256 - MAIN_Fosc / (16 * 波特率)
     24MHz @115200 -> 243 = 0xF3
  UART2/3/4（用 Timer2/3/4，1T 模式）：
     重装值 = 65536 - MAIN_Fosc / (4 * 波特率)
     24MHz @9600   -> 65536 - 625 = 64911 = 0xFD8F
==============================================================*/
#define UART1_BAUD 115200UL
#define UART2_BAUD 9600UL
#define UART3_BAUD 9600UL
#define UART4_BAUD 9600UL

#define BRT1(baud) ((unsigned char)(256UL - (MAIN_Fosc / (16UL * (baud)))))
#define BRT2(baud) ((unsigned int)(65536UL - (MAIN_Fosc / (4UL * (baud)))))

#define UART_TX_TIMEOUT 2000U

static const char code hex_tab[] = "0123456789ABCDEF";

/*==============================================================
  UART1 —— 调试台
==============================================================*/
void UART1_INIT(void) {
    PCON |= 0x80;  /* SMOD = 1 */
    AUXR &= ~0x01; /* S1BRT = 0：用 Timer1 */
    AUXR |= 0x40;  /* T1x12 = 1：Timer1 用 1T */

    SCON = 0x50; /* 模式 1，8 位，允许接收 */

    TMOD &= 0x0F;
    TMOD |= 0x20; /* Timer1，模式 2（8 位自动重装） */

    TH1 = BRT1(UART1_BAUD);
    TL1 = TH1;
    TR1 = 1;

    /* P3.0/P3.1：显式清零 UART1 \xD2\375脚切换（P_SW1 高 2 位） */
    P_SW1 &= ~0xC0;

    P3M1 |= 0x01;
    P3M0 &= ~0x01; /* P3.0 RXD 高阻输入 */
    P3M1 &= ~0x02;
    P3M0 |= 0x02; /* P3.1 TXD 推挽输出 */
}

void UART1_PUTC(unsigned char c) {
    unsigned int guard;

    SBUF = c;
    guard = 0;
    while (!TI)
    {
        guard++;
        if (guard > UART_TX_TIMEOUT)
        {
            break; /* 超时放弃，不死等 */
        }
    }
    TI = 0;
}

void UART1_PUTS(char *s) {
    while (*s)
    {
        UART1_PUTC((unsigned char)(*s));
        s++;
    }
}

void UART1_PUT_U16(unsigned int v) {
    unsigned char buf[6];
    unsigned char n;

    if (v == 0)
    {
        UART1_PUTC('0');
        return;
    }
    n = 0;
    while (v > 0)
    {
        buf[n] = (unsigned char)(v % 10) + '0';
        n++;
        v /= 10;
    }
    while (n > 0)
    {
        n--;
        UART1_PUTC(buf[n]);
    }
}

void UART1_PUT_HEX8(unsigned char v) {
    UART1_PUTC((unsigned char)hex_tab[(v >> 4) & 0x0F]);
    UART1_PUTC((unsigned char)hex_tab[v & 0x0F]);
}

void UART1_NEWLINE(void) {
    UART1_PUTC(0x0D);
    UART1_PUTC(0x0A);
}

unsigned char UART1_GETC(void) {
    unsigned char c;

    if (RI)
    {
        RI = 0;
        c = SBUF;
        return c;
    }
    return 0;
}

/*==============================================================
  UART2 —— GPS（Timer2 做波特率发生器）
==============================================================*/
void UART2_SET_BAUD(unsigned long baud) {
    unsigned int j;

    j = (unsigned int)(65536UL - (MAIN_Fosc / (4UL * baud)));

    AUXR &= ~0x10; /* T2R = 0 */
    AUXR &= ~0x08;
    AUXR |= 0x04; /* 1T */
    T2H = (unsigned char)(j >> 8);
    T2L = (unsigned char)j;
    IE2 &= ~0x04;
    AUXR |= 0x10; /* T2R = 1 */
}

void UART2_INIT(void) {
    P_SW2 &= ~0x01; /* UART2 pins = P1.0/P1.1 */

    S2CON = 0x10; /* 8-bit variable baud + receive enable */
    UART2_SET_BAUD(UART2_BAUD);

    P1M1 |= 0x01;
    P1M0 &= ~0x01; /* P1.0 RXD2 high-Z input */
    P1M1 &= ~0x02;
    P1M0 |= 0x02; /* P1.1 TXD2 push-pull */
}
unsigned char UART2_GETC(void) {
    unsigned char c;

    if (S2CON & 0x01)
    { /* S2RI */
        S2CON &= ~0x01;
        c = S2BUF;
        return c;
    }
    return 0;
}

/*==============================================================
  UART3 —— PMS7003（Timer3 做波特率发生器）
==============================================================*/
void UART3_INIT(void) {
    unsigned int j;

    P_SW2 &= ~0x02; /* UART3 \xD2\375脚 = P5.0/P5.1 */

    j = BRT2(UART3_BAUD);

    S3CON = 0x50; /* 8 位 + 允许接收 + BRT 用 Timer3 */

    T3H = (unsigned char)(j >> 8);
    T3L = (unsigned char)j;
    T4T3M &= 0xF0;
    T4T3M |= 0x0A; /* Timer3：1T、定时、启动 */

    P5M1 |= 0x01;
    P5M0 &= ~0x01; /* P5.0 RXD3 高阻输入 */
    P5M1 &= ~0x02;
    P5M0 |= 0x02; /* P5.1 TXD3 推挽输出 */
}

unsigned char UART3_GETC(void) {
    unsigned char c;

    if (S3CON & 0x01)
    { /* S3RI */
        S3CON &= ~0x01;
        c = S3BUF;
        return c;
    }
    return 0;
}

/*==============================================================
  UART4 —— RS485（Timer4 做波特率发生器）
==============================================================*/
void UART4_SET_BAUD(unsigned long baud) {
    unsigned int j;

    j = (unsigned int)(65536UL - (MAIN_Fosc / (4UL * baud)));
    T4H = (unsigned char)(j >> 8);
    T4L = (unsigned char)j;
}

void UART4_INIT(void) {
    unsigned int j;

    P_SW2 &= ~0x04; /* UART4 \xD2\375脚 = P0.2/P0.3 */

    j = BRT2(UART4_BAUD);

    S4CON = 0x50; /* 8 位 + 允许接收 + BRT 用 Timer4 */

    T4H = (unsigned char)(j >> 8);
    T4L = (unsigned char)j;
    T4T3M &= 0x0F;
    T4T3M |= 0xA0; /* Timer4：1T、定时、启动 */

    P0M1 |= 0x04;
    P0M0 &= ~0x04; /* P0.2 RXD4 高阻输入 */
    P0M1 &= ~0x08;
    P0M0 |= 0x08; /* P0.3 TXD4 推挽输出 */
}

void UART4_PUTC(unsigned char c) {
    unsigned int guard;

    S4BUF = c;
    guard = 0;
    while (!(S4CON & 0x02))
    { /* S4TI */
        guard++;
        if (guard > UART_TX_TIMEOUT)
        {
            break;
        }
    }
    S4CON &= ~0x02;
}

unsigned char UART4_GETC(void) {
    unsigned char c;

    if (S4CON & 0x01)
    { /* S4RI */
        S4CON &= ~0x01;
        c = S4BUF;
        return c;
    }
    return 0;
}

/* RS485 收发方向脚 P2.6，默认下拉为接收 */
void RS485_INIT(void) {
    P2M1 &= ~0x40;
    P2M0 |= 0x40; /* P2.6 推挽输出 */
    RS485_DE = 0; /* 默认接收 */
}
