#include "bsp_log.h"
#include "bsp_bat.h"
#include "bsp_noise.h"
#include "bsp_pms.h"
#include "bsp_rtc.h"
#include "bsp_sens.h"
#include "bsp_wind.h"
#include "ff.h"
#include "spi.h"
#include "tick.h"
#include "uart.h"

/* 记录周期（ms） */
#define LOG_PERIOD_MS 5000U
/* 无卡时重试挂载的间隔（ms） */
#define LOG_RETRY_MS 2000U

static FATFS xdata g_fs;
static FIL xdata g_fil;
static unsigned char g_ok;
static unsigned int g_last;
static unsigned char xdata g_line[140];
static unsigned char g_n;

/*---------------- 行缓冲拼装 ----------------*/
static void put_c(char c) {
    if (g_n < 138U)
    {
        g_line[g_n] = (unsigned char)c;
        g_n++;
    }
}
static void put_s(char *s) {
    while (*s != 0)
    {
        put_c(*s);
        s++;
    }
}
static void put_u(unsigned int v) {
    static unsigned char xdata b[6];
    unsigned char n;

    if (v == 0U)
    {
        put_c('0');
        return;
    }
    n = 0;
    while (v != 0U && n < 5U)
    {
        b[n] = (unsigned char)('0' + (v % 10U));
        n++;
        v /= 10U;
    }
    while (n != 0U)
    {
        n--;
        put_c((char)b[n]);
    }
}
static void put_i(int v) {
    if (v < 0)
    {
        put_c('-');
        v = -v;
    }
    put_u((unsigned int)v);
}
static void put_f10(int v) {
    if (v < 0)
    {
        put_c('-');
        v = -v;
    }
    put_u((unsigned int)(v / 10));
    put_c('.');
    put_c((char)('0' + (v % 10)));
}
static void put_2d(unsigned char v) {
    put_c((char)('0' + (v / 10U)));
    put_c((char)('0' + (v % 10U)));
}

/*----------------------------------------------------------------*/
void bsp_log_init(void) {
    FRESULT fr;
    UINT bw;
    unsigned char rc;

    g_ok = 0;
    g_n = 0;

    rc = TF_INIT();
    if (rc != 0)
    {
        return;
    }

    fr = f_mount(&g_fs, "", 1);
    if (fr != FR_OK)
    {
        return;
    }

    fr = f_open(&g_fil, "LOG.CSV", FA_OPEN_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        return;
    }

    fr = f_lseek(&g_fil, f_size(&g_fil)); /* 追加到末尾 */
    if (fr != FR_OK)
    {
        f_close(&g_fil);
        return;
    }
    if (f_size(&g_fil) == 0)
    {
        f_write(&g_fil,
                "date,time,T(C),RH(%),P(hPa),ALT(m),ALS,MAGx,MAGy,MAGz,CO2(ppm),CO2RH(%),PM1.0,PM2."
                "5,PM10,NOISE(dB),WS(m/s),WD(deg),BAT(V)\r\n",
                113, &bw);
        f_sync(&g_fil);
    }
    g_ok = 1;
    UART1_PUTS("LOG ok\r\n");
}

void bsp_log_tick(void) {
    FRESULT fr;
    UINT bw;
    rtc_time_t t;
    sens_info_t *s;
    wind_info_t *w;
    noise_info_t *nz;
    pms_info_t *p;
    bat_info_t *bat;

    if (!g_ok)
    {
        /* 只在开机时挂载\xB9\375；没成功就每隔几秒重试一次（支持开机后插卡） */
        if ((unsigned int)(g_ms - g_last) >= LOG_RETRY_MS)
        {
            g_last = g_ms;
            bsp_log_init();
        }
        return;
    }
    if ((unsigned int)(g_ms - g_last) < LOG_PERIOD_MS)
    {
        return;
    }
    g_last = g_ms;

    s = bsp_sens_get();
    w = bsp_wind_get();
    nz = bsp_noise_get();
    p = bsp_pms_get();
    bat = bsp_bat_get();

    if (!bsp_rtc_read(&t))
    {
        t.year = 0;
        t.month = 0;
        t.day = 0;
        t.hour = 0;
        t.minute = 0;
        t.second = 0;
    }

    g_n = 0;
    put_s("20");
    put_2d(t.year);
    put_c('-');
    put_2d(t.month);
    put_c('-');
    put_2d(t.day);
    put_c(',');
    put_2d(t.hour);
    put_c(':');
    put_2d(t.minute);
    put_c(':');
    put_2d(t.second);
    put_c(',');

    put_f10(s->sht_t10);
    put_c(',');
    put_f10((int)s->sht_rh10);
    put_c(',');
    put_f10((int)(s->bmp_pa / 10UL));
    put_c(',');
    put_i(s->bmp_alt_m);
    put_c(',');
    put_u(s->als);
    put_c(',');
    put_i(s->mag_x);
    put_c(',');
    put_i(s->mag_y);
    put_c(',');
    put_i(s->mag_z);
    put_c(',');
    put_u(s->co2_ppm);
    put_c(',');
    put_f10((int)s->co2_rh10);
    put_c(',');
    put_u(p->pm1_0);
    put_c(',');
    put_u(p->pm2_5);
    put_c(',');
    put_u(p->pm10);
    put_c(',');
    put_f10((int)nz->db10);
    put_c(',');
    put_f10((int)w->speed10);
    put_c(',');
    put_f10((int)w->dir10);
    put_c(',');
    put_f10((int)(bat->mv / 100));
    put_c('\r');
    put_c('\n');

    fr = f_write(&g_fil, g_line, g_n, &bw);
    if (fr != FR_OK || bw != (UINT)g_n)
    {
        g_ok = 0;
        f_close(&g_fil);
        return;
    }
    f_sync(&g_fil);
}

unsigned char bsp_log_ok(void) {
    return g_ok;
}
