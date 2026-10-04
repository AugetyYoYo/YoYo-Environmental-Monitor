#ifndef _BSP_BAT_H
#define _BSP_BAT_H

#include "STC8H.H"

/*==================================================================
  电池电压 / 电量
  ------------------------------------------------------------------
  硬件：P0.1 = ADC 通道 9，分压 R13=51k / R14=10k，基准 ADC_VRef+
= 2.5V（CJ431）

      VCC_12V(mV) = ADC码 * 15250 / 4096

  为什么一定要取平均：
      本板 ADC 单次采样噪声很大（±180 LSB），但平均值很稳。
      实测 avg16 稳定在 12.0~12.1V，单次则在 11.4~12.8V 之间乱跳。
      详细分析见 doc/测试经验总结.md §2.3。

  电量分档（3S 锂电：12.6V 满 / 9.0V 空），按你要求分 4 档：
      0 ~25%   -> 1 格
      26~50%   -> 2 格
      51~75%   -> 3 格
      76~100%  -> 4 格
      低于 9.0V -> 0 格（代表已经放空）
==================================================================*/

/* 每次更新读几次取平均（单次噪声大，必须平均） */
#define BAT_ADC_SAMPLES 16

/* 3S 锂电的空 / 满电压（mV） */
#define BAT_MV_EMPTY 9000UL
#define BAT_MV_FULL 12600UL

typedef struct {
    unsigned int mv;       /* 电池电压（mV） */
    unsigned char percent; /* 0~100 */
    unsigned char bars;    /* 电量格数 0~4 */
    unsigned char valid;   /* 1 = 读到了；0 = ADC 超时 */
} bat_info_t;

/* 初始化 ADC（复用 adc.c 的 ADC_INIT） */
void bsp_bat_init(void);

/* 更新一次：采样 -> 平均 -> 换算电压 -> 算电量格数 */
void bsp_bat_update(void);

/* 取状态（只读） */
bat_info_t *bsp_bat_get(void);

#endif
