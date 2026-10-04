#ifndef _BSP_MODBUS_H
#define _BSP_MODBUS_H

#include "STC8H.H"

/*==================================================================
  RS485 / Modbus-RTU 主机（UART4 + P2.6 收发控制，9600-8-N-1）
  ------------------------------------------------------------------
  三台从机地址（出厂默认都是 1，需自行改地址）：
      噪声 = 1    风速 = 2    风向 = 3
  改地址帧：01 06 07 D0 00 <新地址> <CRC>
      —— 被改的那台要【单独接在总线上】，改完断电再和别的设备一起接
==================================================================*/

#define MODBUS_BAUD 9600UL
#define MODBUS_TMO_MS 50U /* 单次读超时(ms) */

void bsp_modbus_init(void);

unsigned int modbus_crc16(unsigned char *buf, unsigned char n);

/* 读保持寄存器(0x03)：下发 addr/03/reg/count，返回收到的字节数(0=无应答)。
   rx 由调用者提供，长度需 >= 5 + 2*count */
unsigned char modbus_read(unsigned char addr, unsigned int reg, unsigned char count,
                          unsigned char *rx, unsigned int timeout_ms);

/* 校验应答帧：地址/功能码/字节数/CRC 全对返回 1 */
unsigned char modbus_check(unsigned char addr, unsigned char *rx, unsigned char n,
                           unsigned char count);

/* 把当前地址为 addr 的设备改成 newaddr（发 06 写寄存器 0x07D0） */
void modbus_set_addr(unsigned char newaddr);

/* 扫描地址 1~247，打印有应答者（UART1，调试用） */
void modbus_scan(void);

#endif
