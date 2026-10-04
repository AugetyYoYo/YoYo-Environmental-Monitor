#include "bsp_modbus.h"
#include "tick.h"
#include "bsp_key.h"
#include "uart.h"

void bsp_modbus_init(void) {
    RS485_INIT();
    UART4_INIT();
    UART4_SET_BAUD(MODBUS_BAUD);
}

/* Modbus CRC16：多项式 0xA001，初值 0xFFFF */
unsigned int modbus_crc16(unsigned char *buf, unsigned char n) {
    unsigned int crc;
    unsigned char i;
    unsigned char b;

    crc = 0xFFFF;
    for (i = 0; i < n; i++)
    {
        crc ^= buf[i];
        for (b = 0; b < 8; b++)
        {
            if (crc & 0x0001)
            {
                crc = (unsigned int)((crc >> 1) ^ 0xA001);
            } else
            {
                crc = (unsigned int)(crc >> 1);
            }
        }
    }
    return crc;
}

/* 发 txn 字节，再收最多 rxn 字节，返回实际收到字节\xCA\375。
   收的时候用 S4RI 判断有没有字节（不能用 0 当"没\xCA\375据"，帧里就有 0x00）。 */
static unsigned char modbus_xfer(unsigned char *tx, unsigned char txn, unsigned char *rx,
                                 unsigned char rxn, unsigned int timeout_ms) {
    unsigned char i;
    unsigned char n;
    unsigned int start;
    unsigned int last;

    RS485_TX();
    for (i = 0; i < txn; i++)
    {
        UART4_PUTC(tx[i]);
    }
    delay_ms(1); /* 等最后一位移出去 */
    RS485_RX();

    n = 0;
    start = g_ms;
    last = g_ms;
    while (n < rxn)
    {
        bsp_key_scan(); /* keep scanning keys while waiting for reply */
        if (S4CON & 0x01)
        { /* S4RI */
            rx[n] = UART4_GETC();
            n++;
            last = g_ms;
        } else if ((unsigned int)(g_ms - start) >= timeout_ms)
        {
            break; /* 总超时 */
        } else if ((n > 0) && ((unsigned int)(g_ms - last) >= 5U))
        { break; /* 帧内空闲 >3.5 字符，认为收完 */ }
    }
    return n;
}

unsigned char modbus_read(unsigned char addr, unsigned int reg, unsigned char count,
                          unsigned char *rx, unsigned int timeout_ms) {
    static unsigned char xdata tx[8];
    unsigned int crc;

    tx[0] = addr;
    tx[1] = 0x03;
    tx[2] = (unsigned char)(reg >> 8);
    tx[3] = (unsigned char)reg;
    tx[4] = 0x00;
    tx[5] = count;
    crc = modbus_crc16(tx, 6);
    tx[6] = (unsigned char)crc;
    tx[7] = (unsigned char)(crc >> 8);

    return modbus_xfer(tx, 8, rx, (unsigned char)(5 + 2 * count), timeout_ms);
}

unsigned char modbus_check(unsigned char addr, unsigned char *rx, unsigned char n,
                           unsigned char count) {
    unsigned int crc;

    if (n < (unsigned char)(5 + 2 * count))
    {
        return 0;
    }
    if ((rx[0] != addr) || (rx[1] != 0x03) || (rx[2] != (unsigned char)(2 * count)))
    {
        return 0;
    }
    crc = modbus_crc16(rx, (unsigned char)(3 + 2 * count));
    if ((((unsigned int)rx[4 + 2 * count] << 8) | rx[3 + 2 * count]) != crc)
    {
        return 0;
    }
    return 1;
}

void modbus_set_addr(unsigned char newaddr) {
    static unsigned char xdata tx[8];
    unsigned char i;
    unsigned int crc;

    tx[0] = 0xFF;
    tx[1] = 0x06;
    tx[2] = 0x07;
    tx[3] = 0xD0;
    tx[4] = 0x00;
    tx[5] = newaddr;
    crc = modbus_crc16(tx, 6);
    tx[6] = (unsigned char)crc;
    tx[7] = (unsigned char)(crc >> 8);

    RS485_TX();
    for (i = 0; i < 8; i++)
    {
        UART4_PUTC(tx[i]);
    }
    delay_ms(2);
    RS485_RX();

    UART1_PUTS("set addr -> ");
    UART1_PUTC((unsigned char)('0' + newaddr));
    UART1_NEWLINE();
}

void modbus_scan(void) {
    static unsigned char xdata rx[16];
    unsigned char a;
    unsigned char i;
    unsigned char n;

    UART1_PUTS("scan 1..247 @9600:\r\n");
    for (a = 1; a != 0; a++)
    {
        n = modbus_read(a, 0x0000, 2, rx, 80);
        if (n > 0)
        {
            UART1_PUTS("  a=");
            UART1_PUT_HEX8(a);
            UART1_PUTS(" rx(");
            UART1_PUT_U16((unsigned int)n);
            UART1_PUTS("):");
            for (i = 0; i < n; i++)
            {
                UART1_PUTC(' ');
                UART1_PUT_HEX8(rx[i]);
            }
            UART1_NEWLINE();
        }
    }
    UART1_PUTS("scan done\r\n");
}
