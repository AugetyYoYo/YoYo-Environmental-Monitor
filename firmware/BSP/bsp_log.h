#ifndef _BSP_LOG_H
#define _BSP_LOG_H

#include "STC8H.H"

/*==================================================================
  TF 卡\xCA\375据记录（FatFs）
  ------------------------------------------------------------------
  开机自动挂载 TF 卡，打开/新建根目录下 LOG.CSV，按固定周期追加一行：
      date,time,T(℃),RH(%),P(hPa),ALT(m),ALS,MAGx,MAGy,MAGz,
      CO2(ppm),CO2RH(%),PM1.0,PM2.5,PM10,NOISE(dB),WS(m/s),WD(°)
  记录周期见 bsp_log.c 里的 LOG_PERIOD_MS。
  注意：需先插好 FAT 格式化的卡；卡不在/挂载失败则自动停用，不影响其它功能。
==================================================================*/

/* 初始化：挂载卡、打开 LOG.CSV */
void bsp_log_init(void);

/* 到周期就追加一行；主循环每圈调（非阻塞） */
void bsp_log_tick(void);

/* 1 = \xD5\375在记录 */
unsigned char bsp_log_ok(void);

#endif
