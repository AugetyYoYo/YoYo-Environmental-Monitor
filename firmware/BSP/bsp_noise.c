#include "bsp_noise.h"
#include "bsp_modbus.h"
#include "tick.h"

#define NOISE_ADDR 1 /* 噪声模块地址（默认 1） */
#define NOISE_POLL_MS 1000U

static noise_info_t xdata g_noise;
static unsigned int g_last;

void bsp_noise_init(void) {
    g_noise.ok = 0;
    g_noise.db10 = 0;
    g_last = 0;
}

void bsp_noise_poll(void) {
    static unsigned char xdata rx[8];
    unsigned char n;

    if ((unsigned int)(g_ms - g_last) < NOISE_POLL_MS)
    {
        return;
    }
    g_last = g_ms;

    /* 地址 1，0x0000 读 1 个寄存器：瞬时噪声 ×0.1 dB */
    n = modbus_read(NOISE_ADDR, 0x0000, 1, rx, MODBUS_TMO_MS);
    if (modbus_check(NOISE_ADDR, rx, n, 1))
    {
        unsigned int v = ((unsigned int)rx[3] << 8) | rx[4];
        if ((v >= 100U) && (v <= 1500U))
        { /* 10~150 dB 合理范围 */
            g_noise.db10 = v;
            g_noise.ok = 1;
        } else
        {
            g_noise.ok = 0;
        }
    } else
    {
        g_noise.ok = 0;
    }
}

noise_info_t *bsp_noise_get(void) {
    return &g_noise;
}
