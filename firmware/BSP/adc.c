#include "adc.h"
#include "Config.h"
#include "tick.h"

/* 结果对齐：ADCCFG 的 RESFMT 位（0x20）
   0 = 左对齐：ADC_RES 存高 8 位，ADC_RESL 存低 4 位
   本程序用 0x0F：RESFMT=0（左对齐）+ 最\xC2\375采样              */
#define ADC_TIMEOUT 5000U

void ADC_INIT(void) {
  /* P0.1 设成高阻输入（模拟输入） */
  P0M1 |= 0x02;
  P0M0 &= ~0x02;

  ADCCFG = 0x0F;         /* RESFMT=0 左对齐 + 最\xC2\375采样 */
  ADC_CONTR = ADC_POWER; /* 只开电源 */
  delay_ms(2);           /* 等 ADC 稳定 */
}

unsigned int ADC_READ(unsigned char ch) {
  unsigned int guard;

  /* ★ 必须同时置 ADC_START，否则转换不会开始 */
  ADC_CONTR = ADC_POWER | ADC_START | (ch & 0x0F);

  guard = 0;
  while (!(ADC_CONTR & ADC_FLAG)) {
    guard++;
    if (guard > ADC_TIMEOUT) {
      return 0xFFFF; /* 超时 */
    }
  }
  ADC_CONTR &= ~ADC_FLAG;

  /* RESFMT=0 左对齐：ADC_RES=D11..D4，ADC_RESL 的高 4 位=D3..D0（低 4 位恒 0）
     所以低 4 位要取 ADC_RESL>>4，取 &0x0F 拿到的是恒 0，等于砍掉 4 位分辨率 */
  return ((unsigned int)ADC_RES << 4) | (unsigned int)(ADC_RESL >> 4); /* 0 ~ 4095 */
}

/* VCC_12V(mV) = ADC码 / 4096 * 2.5 * (51k+10k)/10k
           = ADC码 * 0.0037216 V  ->  mV = ADC码 * 15250 / 4096  */
/* 连续读 n 次求平均（n >= 1）。任何一次超时都返回 0xFFFF */
unsigned int ADC_READ_AVG(unsigned char ch, unsigned char n) {
  unsigned long sum;
  unsigned int v;
  unsigned char i;

  sum = 0;
  for (i = 0; i < n; i++) {
    v = ADC_READ(ch);
    if (v == 0xFFFF) {
      return 0xFFFF;
    }
    sum += v;
  }
  return (unsigned int)(sum / n);
}

/* 采样 n 次，给出最小值 / 最大值 / 平均值，用来看抖动到底有多大 */
void ADC_READ_STATS(unsigned char ch, unsigned char n, unsigned int *vmin,
                    unsigned int *vmax, unsigned int *vavg) {
  unsigned long sum;
  unsigned int v;
  unsigned char i;

  sum = 0;
  *vmin = 0xFFFF;
  *vmax = 0;
  for (i = 0; i < n; i++) {
    v = ADC_READ(ch);
    if (v == 0xFFFF) {
      continue;
    }
    if (v < *vmin) {
      *vmin = v;
    }
    if (v > *vmax) {
      *vmax = v;
    }
    sum += v;
  }
  *vavg = (unsigned int)(sum / n);
}
unsigned long ADC_TO_MV_BAT(unsigned int adc) {
  /* VCC_12V(mV) = ADC码/4096 * 2.5V * (51k+10k)/10k
             = ADC码 * 15250 / 4096   (mV)
     3200 码 -> 11914 mV = 11.9V  */
  return (unsigned long)adc * 15250UL / 4096UL;
}