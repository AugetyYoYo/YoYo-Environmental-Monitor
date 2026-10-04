#ifndef __uart_h__
#define __uart_h__

#include "STC8H.H"

/* ---------------- UART1：调试台 ----------------
   P3.0/P3.1 -> CH340 -> USB，115200-8-N-1          */
void UART1_INIT(void);
void UART1_PUTC(unsigned char c);
void UART1_PUTS(char *s);
void UART1_PUT_U16(unsigned int v);
void UART1_PUT_HEX8(unsigned char v);
void UART1_NEWLINE(void);
unsigned char UART1_GETC(void); /* 非阻塞，无数据返回 0 */

/* ---------------- UART2：GPS ----------------
   P1.0(RxD)/P1.1(TxD)，9600-8-N-1，读 NMEA         */
void UART2_INIT(void);
unsigned char UART2_GETC(void);          /* 非阻塞 */
void UART2_SET_BAUD(unsigned long baud); /* switch GPS baud */

/* ---------------- UART3：PMS7003 ----------------
   P5.0(RxD)/P5.1(TxD)，9600-8-N-1                  */
void UART3_INIT(void);
unsigned char UART3_GETC(void); /* 非阻塞 */

/* ---------------- UART4：RS485 ----------------
   P0.2(RxD)/P0.3(TxD)，9600-8-N-1
   收发方向由 P2.6 控制（高=发送），见 RS485_* 宏       */
void UART4_INIT(void);
void UART4_SET_BAUD(unsigned long baud); /* 运行时改波特率（Timer4 重装） */
void UART4_PUTC(unsigned char c);
unsigned char UART4_GETC(void); /* 非阻塞 */

/* RS485 收发方向：P2.6 高=发送，低=接收 */
sbit RS485_DE = P2 ^ 6;
void RS485_INIT(void);
#define RS485_TX() (RS485_DE = 1)
#define RS485_RX() (RS485_DE = 0)

#endif
