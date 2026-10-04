#include "bsp_bat.h"
#include "adc.h"

/* 电池状态（放 xdata 省 DATA） */
static bat_info_t xdata g_bat;

void bsp_bat_init(void) {
    ADC_INIT();

    g_bat.mv = 0;
    g_bat.percent = 0;
    g_bat.bars = 0;
    g_bat.valid = 0;
}

void bsp_bat_update(void) {
    unsigned int adc_code;
    unsigned long mv;

    /* 采样 BAT_ADC_SAMPLES 次取平均（单次采样噪声大） */
    adc_code = ADC_READ_AVG(ADC_CH_BAT, BAT_ADC_SAMPLES);
    if (adc_code == 0xFFFF)
    {
        g_bat.valid = 0;
        return;
    }

    mv = ADC_TO_MV_BAT(adc_code);
    g_bat.mv = (unsigned int)mv;
    g_bat.valid = 1;

    /* 电压 -> 百分比（线性） */
    if (mv <= BAT_MV_EMPTY)
    {
        g_bat.percent = 0;
    } else if (mv >= BAT_MV_FULL)
    {
        g_bat.percent = 100;
    } else
    {
        g_bat.percent =
            (unsigned char)(((mv - BAT_MV_EMPTY) * 100UL) / (BAT_MV_FULL - BAT_MV_EMPTY));
    }

    /* 百分比 -> 4 格 */
    if (mv < BAT_MV_EMPTY)
    {
        g_bat.bars = 0;
    } else if (g_bat.percent >= 76)
    {
        g_bat.bars = 4;
    } else if (g_bat.percent >= 51)
    {
        g_bat.bars = 3;
    } else if (g_bat.percent >= 26)
    {
        g_bat.bars = 2;
    } else
    {
        g_bat.bars = 1;
    }
}

bat_info_t *bsp_bat_get(void) {
    return &g_bat;
}
