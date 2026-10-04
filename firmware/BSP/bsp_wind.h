#ifndef _BSP_WIND_H
#define _BSP_WIND_H

#include "STC8H.H"

/*==================================================================
  风速风向（RS485 Modbus-RTU，9600-8-N-1）
      风速传感器 ZTS-3000-FSJT 地址 2：0x0000=风速×0.1m/s，0x0001=风级
      风向传感器 ZTS-3000-FXJT 地址 3：0x0000=角度×10(0~3599)，0x0001=整数角度
==================================================================*/

typedef struct {
    unsigned char ok;     /* 风速/风级是否有效 */
    unsigned int speed10; /* 风速 ×0.1（36 = 3.6 m/s） */
    unsigned char level;  /* 风级 */
    unsigned char dir_ok; /* 风向是否有效 */
    unsigned int dir10;   /* 风向 ×0.1（1608 = 160.8°） */
} wind_info_t;

void bsp_wind_init(void);

/* 每 1 秒读一次 风速(地址2) + 风向(地址3)，主循环每圈调 */
void bsp_wind_poll(void);

wind_info_t *bsp_wind_get(void);

#endif
