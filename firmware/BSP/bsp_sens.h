#ifndef _BSP_SENS_H
#define _BSP_SENS_H

#include "STC8H.H"

/*==================================================================
  传感器采集（5 个 I2C 从机，统一在这里管）
  ------------------------------------------------------------------
  地址（硬件文档 4.1 节）：
      IST8310  0x0C  磁力计
      VEML7700 0x10  光照
      SHT45    0x44  温湿度
      PCF8563  0x51  RTC（在 bsp_rtc 里管，这里不管）
      SCD41    0x62  CO2

  设计要点：
      * 每个传感器读一次都要花时间（SHT45 要等 12ms，SCD41 要等 2ms），
        所以做成【轮流刷新】：bsp_sens_update_next() 每次只刷新一个，
        主循环每圈调一次，谁也不会把界面卡住。
      * SCD41 必须先 start_periodic_measurement，而且 5 秒才出一个数据，
        没新数据时传感器会 NACK —— 这不是错误，静默跳过即可。
      * 所有数值都是定点：温度/湿度用 ×10 的整数（272 表示 27.2）。
        8051 上没有 FPU，浮点运算又慢又占空间，能不用就不用。
==================================================================*/

/* 轮流刷新的顺序 */
typedef enum {
    sens_id_sht45 = 0, /* 温湿度 */
    sens_id_bmp581,    /* 气压 + 海拔 */
    sens_id_veml7700,  /* 光照 */
    sens_id_ist8310,   /* 磁力计 */
    sens_id_scd41,     /* CO2 */
    sens_id_max
} sens_id_t;

/* 所有传感器的当前值 */
typedef struct {
    /* --- SHT45 温湿度 --- */
    unsigned char sht_ok;
    int sht_t10;            /* 温度 ×10（272 = 27.2°C），可以为负 */
    unsigned int sht_rh10; /* 湿度 ×10（591 = 59.1%） */

    /* --- BMP581 气压 / 海拔 --- */
    unsigned char bmp_ok;
    unsigned long bmp_pa; /* 气压（Pa） */
    int bmp_alt_m;        /* 海拔（米，相对海平面） */

    /* --- VEML7700 光照 --- */
    unsigned char als_ok;
    unsigned int als; /* ALS 原始计数（越大越亮） */

    /* --- IST8310 磁力计 --- */
    unsigned char mag_ok;
    int mag_x;
    int mag_y;
    int mag_z;

    /* --- SCD41 CO2 --- */
    unsigned char co2_ok;
    unsigned int co2_ppm;
    int co2_t10;
    unsigned int co2_rh10;
} sens_info_t;

/* 初始化 I2C 上的 5 个传感器（含启动 SCD41 周期测量） */
void bsp_sens_init(void);

/* 轮流刷新一个传感器；主循环每圈调一次即可 */
void bsp_sens_update_next(void);

/* 取状态（只读） */
sens_info_t *bsp_sens_get(void);

#endif
