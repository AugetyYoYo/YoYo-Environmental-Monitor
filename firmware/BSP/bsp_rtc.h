#ifndef _BSP_RTC_H
#define _BSP_RTC_H

#include "STC8H.H"

/*==================================================================
  实时时钟 PCF8563（I2C 地址 0x51）
  ------------------------------------------------------------------
  寄存器（全部是 BCD 码）：
      0x00 控制寄存器1   0x01 控制寄存器2
      0x02 秒（bit7 = VL）  0x03 分   0x04 时（bit6=0 表示 24 小时制）
      0x05 日   0x06 星期   0x07 月（bit7 = 世纪位，0 表示 20xx）  0x08 年
  几个关键点：
      * bit7 of 0x02 是 VL（Voltage Low）标志：没有备电、掉过电就置 1，
        表示"时间不可信"。写 0 即清除，所以授时的时候顺便把它清掉。
      * 本板【没有备电】，断电时间就丢。正式固件应该在开机后用 GPS 授时一次。
      * 星期寄存器是独立的，不会被年月日自动推算，写时间时要自己算好写进去。
==================================================================*/

/* 时间结构体 */
typedef struct {
    unsigned char year;    /* 年：0~99，代表 2000~2099 */
    unsigned char month;   /* 月：1~12 */
    unsigned char day;     /* 日：1~31 */
    unsigned char hour;    /* 时：0~23 */
    unsigned char minute;  /* 分：0~59 */
    unsigned char second;  /* 秒：0~59 */
    unsigned char weekday; /* 星期：0=周日 ... 6=周六 */
    unsigned char valid;   /* 1 = VL 为 0，时间有效；0 = 时间无效 */
} rtc_time_t;

/* 初始化 I2C 上的 RTC：只清控制寄存器，不动时间 */
void bsp_rtc_init(void);

/* 读当前时间；成功返回 1（I2C 失败返回 0，此时 t 里的内容无意义） */
unsigned char bsp_rtc_read(rtc_time_t *t);

/* 写时间：自动算星期、清 VL、设 24 小时制、世纪位 = 0（20xx） */
void bsp_rtc_write(rtc_time_t *t);

/* 算某天是星期几：0=周日 ... 6=周六（Sakamoto 算法） */
unsigned char bsp_rtc_weekday(unsigned char year, unsigned char month, unsigned char day);

#endif
