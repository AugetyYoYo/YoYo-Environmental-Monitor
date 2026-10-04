#include "bsp_wind.h"
#include "bsp_modbus.h"
#include "tick.h"

#define WIND_ADDR_SPEED 2  /* 风速传感器地址 */
#define WIND_ADDR_DIR 3    /* 风向传感器地址 */
#define WIND_POLL_MS 1000U /* 每 1 秒读一次 */

static wind_info_t xdata g_wind;
static unsigned int g_last;

void bsp_wind_init(void) {
    g_wind.ok = 0;
    g_wind.speed10 = 0;
    g_wind.level = 0;
    g_wind.dir_ok = 0;
    g_wind.dir10 = 0;
    g_last = 0;
}

void bsp_wind_poll(void) {
    static unsigned char xdata rx[12];
    unsigned char n;

    if ((unsigned int)(g_ms - g_last) < WIND_POLL_MS)
    {
        return;
    }
    g_last = g_ms;

    /* --- 风速 + 风级（地址 2，0x0000 起 2 个） --- */
    n = modbus_read(WIND_ADDR_SPEED, 0x0000, 2, rx, MODBUS_TMO_MS);
    if (modbus_check(WIND_ADDR_SPEED, rx, n, 2))
    {
        unsigned int sp = ((unsigned int)rx[3] << 8) | rx[4];
        unsigned char lv = (unsigned char)(((unsigned int)rx[5] << 8) | rx[6]);
        if ((sp <= 600U) && (lv <= 17U))
        { /* 量程 0~60 m/s */
            g_wind.speed10 = sp;
            g_wind.level = lv;
            g_wind.ok = 1;
        } else
        {
            g_wind.ok = 0;
        }
    } else
    {
        g_wind.ok = 0;
    }

    /* --- 风向（地址 3，0x0000 = 角度×10） --- */
    n = modbus_read(WIND_ADDR_DIR, 0x0000, 2, rx, MODBUS_TMO_MS);
    if (modbus_check(WIND_ADDR_DIR, rx, n, 2))
    {
        unsigned int d = ((unsigned int)rx[3] << 8) | rx[4];
        if (d <= 3599U)
        {
            g_wind.dir10 = d;
            g_wind.dir_ok = 1;
        } else
        {
            g_wind.dir_ok = 0;
        }
    } else
    {
        g_wind.dir_ok = 0;
    }
}

wind_info_t *bsp_wind_get(void) {
    return &g_wind;
}
