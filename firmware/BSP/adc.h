#ifndef __adc_h__
#define __adc_h__

#include "STC8H.H"

/* ADC 通道：9 = P0.1 = ADC_BAT
   基准 ADC_VRef+ = 2.5V（CJ431）
   分压 R13=51k / R14=10k
   VCC_12V(mV) = ADC码 * 15250 / 4096                              */
#define ADC_CH_BAT 9

void ADC_INIT(void);

/* 读一次 ADC，返回 0~4095；超时返回 0xFFFF */
unsigned int ADC_READ(unsigned char ch);

/* 把 ADC 码换算成 VCC_12V 的毫伏值 */
/* 连续读 n 次求平均 */
unsigned int ADC_READ_AVG(unsigned char ch, unsigned char n);

/* 采样 n 次，给出 min/max/avg */
void ADC_READ_STATS(unsigned char ch, unsigned char n, unsigned int *vmin, unsigned int *vmax, unsigned int *vavg);

unsigned long ADC_TO_MV_BAT(unsigned int adc);

#endif
