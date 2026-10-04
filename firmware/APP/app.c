#include "app.h"
#include "adc.h"
#include "bsp_bat.h"
#include "bsp_gps.h"
#include "bsp_log.h"
#include "bsp_modbus.h"
#include "bsp_noise.h"
#include "bsp_pms.h"
#include "bsp_rtc.h"
#include "bsp_sens.h"
#include "bsp_ui.h"
#include "bsp_wind.h"
#include "city_table.h"
#include "buzzer.h"
#include "i2c.h"
#include "led.h"
#include "oled.h"
#include "spi.h"
#include "tick.h"
#include "uart.h"

/*==================================================================
  菜单项
==================================================================*/
typedef enum {
  menu_rtc = 0,  /* 时间 */
  menu_gps,      /* GPS 定位 */
  menu_bat,      /* 电池 */
  menu_th,       /* 温湿度 */
  menu_alt,      /* 海拔（BMP581） */
  menu_als,      /* 光照 */
  menu_mag,      /* 磁场 */
  menu_co2,      /* CO2 */
  menu_pm,       /* 粉尘（PMS7003） */
  menu_noise,    /* 噪声（RS485） */
  menu_wind,     /* 风速风向（RS485） */
  menu_selftest, /* 系统自检 */
  menu_max
} menu_id_t;

/* 菜单一屏显示 3 项（第 0 行留给标题） */
#define MENU_ROWS 3

/*==================================================================
  内部状态
==================================================================*/
static page_id_t g_page;       /* 当前页面 */
static unsigned char g_sel;    /* 菜单里选中的项 */
static unsigned char g_top;    /* 菜单滚动到第几项开始显示 */
static unsigned char g_redraw; /* 1 = 需要重画当前页 */

static rtc_time_t xdata g_rtc; /* RTC 读出来的时间 */
static unsigned char g_rtc_ok; /* RTC 读成功\271\375 */

/* 字符串拼装缓冲（拼一行的显示内容用） */
static char xdata g_lb[26];
static unsigned char g_lbn;

/* 页面重画用的定时 */
static unsigned int g_last_1s;
static unsigned int g_last_10s;
static unsigned char g_blink; /* 闪烁相位，每 500ms 翻转 */

/*==================================================================
  小工具
==================================================================*/

/* 字符串拼装：清空 */
static void lb_clr(void) {
  g_lbn = 0;
  g_lb[0] = 0;
}

/* 字符串拼装：追加一个字符串 */
static void lb_str(char *s) {
  while (*s != 0 && g_lbn < 24) {
    g_lb[g_lbn] = *s;
    g_lbn = (unsigned char)(g_lbn + 1);
    s++;
  }
  g_lb[g_lbn] = 0;
}

/* 字符串拼装：追加一个 ×10 的定点\312\375（272 -> "27.2"） */
static void lb_10(int v) { lb_str(bsp_ui_fmt10(v)); }

/* 字符串拼装：追加一个无符号\312\375 */
static void lb_u(unsigned int v) { lb_str(bsp_ui_fmtu(v)); }

/* 追加有符号整\312\375（海拔可能为负） */
static void lb_i(int v) {
  if (v < 0) {
    lb_str(" -");
    v = -v;
  }
  lb_u((unsigned int)v);
}

/* 取菜单项名字 */
static char *menu_name(unsigned char id) {
  switch ((menu_id_t)id) {
  case menu_rtc:
    return "1 时间";
  case menu_gps:
    return "2 GPS定位";
  case menu_bat:
    return "3 电池电压";
  case menu_th:
    return "4 温湿度";
  case menu_alt:
    return "5 海拔";
  case menu_als:
    return "6 光照";
  case menu_mag:
    return "7 磁场";
  case menu_co2:
    return "8 CO2";
  case menu_pm:
    return "9 粉尘";
  case menu_noise:
    return "10 噪声";
  case menu_wind:
    return "11 风速风向";
  case menu_selftest:
    return "12 系统自检";
  default:
    return "?";
  }
}

/* 星期名字 */
static char *weekday_name(unsigned char wd) {
  switch (wd) {
  case 0:
    return "周日";
  case 1:
    return "周一";
  case 2:
    return "周二";
  case 3:
    return "周\310\375";
  case 4:
    return "周四";
  case 5:
    return "周五";
  case 6:
    return "周六";
  default:
    return "??";
  }
}

/* 两位\312\375字（补零）追加进缓冲 */
static void lb_2d(unsigned char v) {
  g_lb[g_lbn] = (char)('0' + (v / 10));
  g_lbn = (unsigned char)(g_lbn + 1);
  g_lb[g_lbn] = (char)('0' + (v % 10));
  g_lbn = (unsigned char)(g_lbn + 1);
  g_lb[g_lbn] = 0;
}

/* 追加 4 位补零\312\375字（0000-9999） */
static void lb_4d(unsigned int v) {
  g_lb[g_lbn] = (char)('0' + (v / 1000U) % 10U);
  g_lbn = (unsigned char)(g_lbn + 1);
  g_lb[g_lbn] = (char)('0' + (v / 100U) % 10U);
  g_lbn = (unsigned char)(g_lbn + 1);
  g_lb[g_lbn] = (char)('0' + (v / 10U) % 10U);
  g_lbn = (unsigned char)(g_lbn + 1);
  g_lb[g_lbn] = (char)('0' + (v % 10U));
  g_lbn = (unsigned char)(g_lbn + 1);
  g_lb[g_lbn] = 0;
}

/* 用磁力计 X/Y 计算方位角 0~359°（0°=+X 轴，顺时针为\325\375，近似） */
static unsigned int mag_heading(int dx, int dy) {
  int ax;
  int ay;
  int a;
  unsigned int h;

  if (dx == 0 && dy == 0) {
    return 0;
  }
  ax = (dx < 0) ? -dx : dx;
  ay = (dy < 0) ? -dy : dy;
  if (ax >= ay) {
    a = (int)((long)ay * 45L / ax);
    if (dx >= 0) {
      h = (unsigned int)((dy >= 0) ? a : (360 - a));
    } else {
      h = (unsigned int)((dy >= 0) ? (180 - a) : (180 + a));
    }
  } else {
    a = (int)((long)ax * 45L / ay);
    if (dy >= 0) {
      h = (unsigned int)((dx >= 0) ? (90 - a) : (90 + a));
    } else {
      h = (unsigned int)((dx >= 0) ? (270 + a) : (270 - a));
    }
  }
  return (unsigned int)(h % 360U);
}

/* 方位角 -> 八方位名称 */
static char *compass_name(unsigned int h) {
  if (h < 23U || h >= 338U) {
    return "北";
  }
  if (h < 68U) {
    return "东北";
  }
  if (h < 113U) {
    return "东";
  }
  if (h < 158U) {
    return "东南";
  }
  if (h < 203U) {
    return "南";
  }
  if (h < 248U) {
    return "西南";
  }
  if (h < 293U) {
    return "西";
  }
  return "西北";
}

/*==================================================================
  主页面
==================================================================*/
static void draw_home(void) {
  bsp_ui_clear();

  /* --- 第 0 行：时间 + 电池条 --- */
  lb_clr();
  if (g_rtc_ok && g_rtc.valid) {
    lb_2d(g_rtc.hour);
    lb_str(":");
    lb_2d(g_rtc.minute);
    lb_str(":");
    lb_2d(g_rtc.second);
  } else {
    lb_str("--:--:--");
  }
  bsp_ui_line(0, g_lb);

  {
    bat_info_t *b = bsp_bat_get();
    /* 电量低于 10% 时闪烁告警 */
    bsp_ui_bars(0, 88, b->bars, (unsigned char)((b->percent < 10) && g_blink));
  }

  /* --- 第 1 行：日期 + 星期 --- */
  lb_clr();
  if (g_rtc_ok && g_rtc.valid) {
    lb_str("20");
    lb_2d(g_rtc.year);
    lb_str("-");
    lb_2d(g_rtc.month);
    lb_str("-");
    lb_2d(g_rtc.day);
    lb_str(" ");
    lb_str(weekday_name(g_rtc.weekday));
  } else {
    lb_str("时间未校准");
  }
  bsp_ui_line(2, g_lb);

  /* --- 第 2 行：GPS 状态 --- */
  {
    gps_info_t *g = bsp_gps_get();
    lb_clr();
    if (g->fix) {
      lb_str("GPS定位 ");
      lb_u(g->sats_used);
      lb_str("颗");
    } else {
      lb_str("GPS搜星 ");
      lb_u(g->sats_view);
      lb_str("颗");
    }
    bsp_ui_line(4, g_lb);
  }

  /* --- 第 3 行：温度 / 湿度 / 海拔 --- */
  {
    sens_info_t *s = bsp_sens_get();
    lb_clr();
    lb_str("T");
    if (s->sht_ok) {
      lb_10(s->sht_t10);
    } else {
      lb_str("--");
    }
    lb_str(" H");
    if (s->sht_ok) {
      lb_u((unsigned int)(s->sht_rh10 / 10));
    } else {
      lb_str("--");
    }
    lb_str("% ");
    if (s->bmp_ok) {
      lb_i(s->bmp_alt_m);
    } else {
      lb_str("----");
    }
    lb_str("m");
    bsp_ui_line(6, g_lb);
  }
}

/*==================================================================
  菜单
==================================================================*/
static void draw_menu(void) {
  unsigned char i;
  unsigned char id;

  bsp_ui_clear();
  bsp_ui_line(0, "UP/DN选 长按UP进");

  for (i = 0; i < MENU_ROWS; i++) {
    id = (unsigned char)(g_top + i);
    if (id >= (unsigned char)menu_max) {
      bsp_ui_line((unsigned char)(2 + i * 2), "");
      continue;
    }
    lb_clr();
    if (id == g_sel) {
      lb_str(">");
    } else {
      lb_str(" ");
    }
    lb_str(menu_name(id));
    bsp_ui_line((unsigned char)(2 + i * 2), g_lb);
  }
}

/*==================================================================
  \312\375据页
==================================================================*/
/*==================================================================
  系统自检
  ------------------------------------------------------------------
  把平时用不到、容易在焊接功能的模块跑一遍，出错就打断，OLED 给出结果。
  这里放全局变量，而不是函\312\375里放这些栈；因为这些字"自检结果"后它们的局部变量，
  都在 DATA 里反复用，8051 里链接器不会给未调用的函\312\375做覆盖，
  每次调用的局部都占一份 DATA，容易把 128 字节串爆了。
==================================================================*/
static unsigned char g_st_done;
static unsigned char g_st_tf;
static unsigned char g_st_i2c;
static unsigned char g_st_adc;
static unsigned char xdata g_st_sec[512];
static unsigned char xdata g_st_list[12];

static void selftest_run(void) {
  unsigned char rc;
  unsigned int lo;
  unsigned int hi;
  unsigned int av;
  unsigned int t;

  UART1_PUTS("--- selftest ---\r\n");

  /* --- I2C 总线，扫描所有从机 --- */
  g_st_i2c = I2C_SCAN(g_st_list, 12);
  UART1_PUTS("I2C found ");
  UART1_PUT_U16((unsigned int)g_st_i2c);
  UART1_PUTS("\r\n");

  /* --- ADC 采样统计 --- */
  ADC_READ_STATS(ADC_CH_BAT, 32, &lo, &hi, &av);
  g_st_adc = (unsigned char)((hi - lo) < 500U);
  UART1_PUTS("ADC spread=");
  UART1_PUT_U16((unsigned int)(hi - lo));
  UART1_PUTS("\r\n");

  /* --- TF 卡初始化 + 读扇区 0 的 MBR 签名 --- */
  UART1_PUTS("TF_CD=");
  UART1_PUTC(TF_CD ? '1' : '0');
  UART1_PUTS("\r\n");
  t = SPI_CLOCK_MS();
  UART1_PUTS("SPI 2000B=");
  UART1_PUT_U16(t);
  UART1_PUTS("ms\r\n");

  rc = TF_INIT();
  if (rc == 0) {
    rc = TF_READ_SECTOR(0x00000000UL, g_st_sec);
    g_st_tf = (unsigned char)((rc == 0 && g_st_sec[510] == 0x55 &&
                               g_st_sec[511] == 0xAA)
                                  ? 1
                                  : 0);
  } else {
    g_st_tf = 0;
  }
  UART1_PUTS("TF ");
  UART1_PUTS(g_st_tf ? "PASS" : "FAIL");
  UART1_PUTS("\r\n");

  /* --- OLED 自检图像 --- */
  OLED_TEST();
  OLED_CLEAR();
  OLED_SHOW_U16(0, 0, 12345);
  OLED_SHOW_DIGITS(7);

  /* --- 蜂鸣器响一下 --- */
  BUZZER_ON();
  delay_ms(80);
  BUZZER_OFF();
}
static void draw_data(void) {
  bsp_ui_clear();
  bsp_ui_line(0, menu_name(g_sel));

  switch ((menu_id_t)g_sel) {
  case menu_rtc:
    if (g_rtc_ok) {
      lb_clr();
      lb_str("20");
      lb_2d(g_rtc.year);
      lb_str("-");
      lb_2d(g_rtc.month);
      lb_str("-");
      lb_2d(g_rtc.day);
      lb_str(" ");
      lb_str(weekday_name(g_rtc.weekday));
      bsp_ui_line(2, g_lb);

      lb_clr();
      lb_2d(g_rtc.hour);
      lb_str(":");
      lb_2d(g_rtc.minute);
      lb_str(":");
      lb_2d(g_rtc.second);
      bsp_ui_line(4, g_lb);

      if (g_rtc.valid) {
        bsp_ui_line(6, "VL=1 时间有效");
      } else {
        bsp_ui_line(6, "VL=0 时间无效");
      }
    } else {
      bsp_ui_line(2, "PCF8563 读不到");
    }
    break;

  case menu_gps: {
    gps_info_t *g = bsp_gps_get();
    city_show(0, g);

    lb_clr();
    if (g->lat_ns == 'N' || g->lat_ns == 'S' || g->lat_d != 0U) {
      lb_str("纬");
      if (g->lat_ns == 'S') {
        lb_str("S");
      } else {
        lb_str("N");
      }
      lb_u(g->lat_d);
      lb_str(".");
      lb_4d(g->lat_f4);
    } else {
      lb_str("--");
    }
    bsp_ui_line(2, g_lb);

    lb_clr();
    if (g->lon_ew == 'E' || g->lon_ew == 'W' || g->lon_d != 0U) {
      lb_str("经");
      if (g->lon_ew == 'W') {
        lb_str("W");
      } else {
        lb_str("E");
      }
      lb_u(g->lon_d);
      lb_str(".");
      lb_4d(g->lon_f4);
    } else {
      lb_str("--");
    }
    bsp_ui_line(4, g_lb);

    lb_clr();
    if (g->lines == 0) {
      lb_str("无 GPS 信号");
    } else {
      lb_str("星");
      lb_u(g->sats_used);
      lb_str("/");
      lb_u(g->sats_view);
      lb_str(" ");
      lb_str("天线");
      if (g->ant == 1) {
        lb_str("\325\375常");
      } else if (g->ant == 2) {
        lb_str("开路");
      } else if (g->ant == 3) {
        lb_str("短路");
      } else {
        lb_str("--");
      }
    }
    bsp_ui_line(6, g_lb);
    break;
  }

  case menu_bat: {
    bat_info_t *b = bsp_bat_get();
    lb_clr();
    lb_str("电压 ");
    lb_u((unsigned int)(b->mv / 1000));
    lb_str(".");
    lb_u((unsigned int)((b->mv % 1000) / 100));
    lb_str(" V");
    bsp_ui_line(2, g_lb);

    lb_clr();
    lb_str("电量 ");
    lb_u((unsigned int)b->percent);
    lb_str(" %");
    bsp_ui_line(4, g_lb);

    if (b->valid) {
      bsp_ui_line(6, "ADC \325\375常");
    } else {
      bsp_ui_line(6, "ADC 超时");
    }
    break;
  }

  case menu_th: {
    sens_info_t *s = bsp_sens_get();
    lb_clr();
    lb_str("温度 ");
    lb_10(s->sht_t10);
    lb_str(" C");
    bsp_ui_line(2, g_lb);

    lb_clr();
    lb_str("湿度 ");
    lb_10(s->sht_rh10);
    lb_str(" %");
    bsp_ui_line(4, g_lb);

    if (s->sht_ok) {
      bsp_ui_line(6, "SHT45 \325\375常");
    } else {
      bsp_ui_line(6, "SHT45 读不到");
    }
    break;
  }

  case menu_co2: {
    sens_info_t *s = bsp_sens_get();
    lb_clr();
    lb_str("CO2 ");
    if (s->co2_ok) {
      lb_u(s->co2_ppm);
      lb_str(" ppm");
    } else {
      lb_str("--");
    }
    bsp_ui_line(2, g_lb);

    lb_clr();
    lb_str("温度 ");
    lb_10(s->co2_t10);
    lb_str(" C");
    bsp_ui_line(4, g_lb);

    lb_clr();
    lb_str("湿度 ");
    lb_10(s->co2_rh10);
    lb_str(" %");
    bsp_ui_line(6, g_lb);
    break;
  }

  case menu_alt: {
    sens_info_t *s = bsp_sens_get();
    lb_clr();
    lb_str("海拔 ");
    lb_i(s->bmp_alt_m);
    lb_str(" m");
    bsp_ui_line(2, g_lb);

    lb_clr();
    lb_str("气压 ");
    lb_10((int)(s->bmp_pa / 10UL));
    lb_str(" hPa");
    bsp_ui_line(4, g_lb);

    if (s->bmp_ok) {
      bsp_ui_line(6, "BMP581 \325\375常");
    } else {
      bsp_ui_line(6, "BMP581 读不到");
    }
    break;
  }

  case menu_als: {
    sens_info_t *s = bsp_sens_get();
    unsigned int a = s->als;

    lb_clr();
    lb_str("ALS 值 ");
    lb_u(a);
    bsp_ui_line(2, g_lb);
    if (s->als_ok) {
      bsp_ui_line(4, "VEML7700 \325\375常");
    } else {
      bsp_ui_line(4, "VEML7700 读不到");
    }

    if (!s->als_ok) {
      bsp_ui_line(6, "光强 --");
    } else if (a < 20U) {
      bsp_ui_line(6, "光强 很暗");
    } else if (a < 200U) {
      bsp_ui_line(6, "光强 偏暗");
    } else if (a < 4000U) {
      bsp_ui_line(6, "光强 适中");
    } else if (a < 20000U) {
      bsp_ui_line(6, "光强 明亮");
    } else {
      bsp_ui_line(6, "光强 刺眼");
    }
    break;
  }

  case menu_mag: {
    sens_info_t *s = bsp_sens_get();
    unsigned int h;

    lb_clr();
    lb_str("X ");
    lb_10(s->mag_x);
    bsp_ui_line(2, g_lb);

    lb_clr();
    lb_str("Y ");
    lb_10(s->mag_y);
    bsp_ui_line(4, g_lb);

    lb_clr();
    lb_str("Z ");
    lb_10(s->mag_z);
    bsp_ui_line(6, g_lb);

    /* 右侧：磁方位（X/Y 计算） */
    h = mag_heading(s->mag_x, s->mag_y);
    OLED_SHOW_STR(2, 72, (unsigned char *)"方位");
    OLED_SHOW_STR(4, 72, (unsigned char *)compass_name(h));
    lb_clr();
    lb_u(h);
    lb_str("度");
    OLED_SHOW_STR(6, 72, (unsigned char *)g_lb);
    break;
  }

  case menu_pm: {
    pms_info_t *p = bsp_pms_get();
    lb_clr();
    lb_str("PM1.0 ");
    lb_u(p->pm1_0);
    bsp_ui_line(2, g_lb);

    lb_clr();
    lb_str("PM2.5 ");
    lb_u(p->pm2_5);
    bsp_ui_line(4, g_lb);

    lb_clr();
    lb_str("PM10  ");
    lb_u(p->pm10);
    bsp_ui_line(6, g_lb);
    break;
  }

  case menu_noise: {
    noise_info_t *nz = bsp_noise_get();
    lb_clr();
    lb_str("噪声 ");
    if (nz->ok) {
      lb_10((int)nz->db10);
      lb_str(" dB");
    } else {
      lb_str("--");
    }
    bsp_ui_line(2, g_lb);

    if (!nz->ok) {
      bsp_ui_line(4, "噪声 读不到");
      bsp_ui_line(6, "");
    } else {
      bsp_ui_line(4, "噪声 \325\375常");
      if (nz->db10 < 4000U) {
        bsp_ui_line(6, "环境 安静");
      } else if (nz->db10 < 5000U) {
        bsp_ui_line(6, "环境 较静");
      } else if (nz->db10 < 6000U) {
        bsp_ui_line(6, "环境 适中");
      } else if (nz->db10 < 7000U) {
        bsp_ui_line(6, "环境 嘈杂");
      } else {
        bsp_ui_line(6, "环境 很吵");
      }
    }
    break;
  }

  case menu_wind: {
    wind_info_t *w = bsp_wind_get();
    lb_clr();
    lb_str("风速 ");
    lb_10((int)w->speed10);
    lb_str(" m/s");
    bsp_ui_line(2, g_lb);

    lb_clr();
    lb_str("风级 ");
    lb_u((unsigned int)w->level);
    lb_str(" 级");
    bsp_ui_line(4, g_lb);

    lb_clr();
    lb_str("风向 ");
    if (w->dir_ok) {
      lb_10((int)w->dir10);
      lb_str(" 度");
    } else {
      lb_str("--");
    }
    bsp_ui_line(6, g_lb);
    break;
  }

  case menu_selftest:
    if (!g_st_done) {
      g_st_done = 1;
      selftest_run();
    }
    if (g_st_tf) {
      bsp_ui_line(2, "TF 自检 PASS");
    } else {
      bsp_ui_line(2, "TF 自检 FAIL");
    }
    lb_clr();
    lb_str("I2C ");
    lb_u((unsigned int)g_st_i2c);
    lb_str(" 个从机");
    bsp_ui_line(4, g_lb);
    if (g_st_adc) {
      bsp_ui_line(6, "ADC \325\375常");
    } else {
      bsp_ui_line(6, "ADC \312\375据偏移");
    }
    break;

  default:
    bsp_ui_line(2, "未实现");
    break;
  }
}

/* 按当前页重画 */
static void app_draw(void) {
  if (g_page == page_home) {
    draw_home();
  } else if (g_page == page_menu) {
    draw_menu();
  } else {
    draw_data();
  }
}

/*==================================================================
  按键处理
==================================================================*/
static void app_on_key(key_evt_t evt) {
  switch (evt) {
  case key_evt_mode_short:
    /* MODE 短按：主页 <-> 菜单进入/返回 */
    if (g_page == page_home) {
      g_page = page_menu;
    } else if (g_page == page_menu) {
      g_page = page_home;
    } else {
      g_page = page_menu;
    }
    g_redraw = 1;
    BUZZER_BEEP(20);
    break;

  case key_evt_mode_long:
    /* MODE 长按：只在主页有效 —— 强制刷新全部 */
    if (g_page == page_home) {
      BUZZER_BEEP(50);
      bsp_ui_line(6, "刷新中...");
      bsp_rtc_read(&g_rtc);
      g_rtc_ok = 1;
      bsp_bat_update();
      bsp_gps_poll();
      g_redraw = 1;
    }
    break;

  case key_evt_up_short:
    /* UP 短按：菜单上移选中 */
    if (g_page == page_menu) {
      if (g_sel > 0) {
        g_sel--;
        if (g_sel < g_top) {
          g_top = g_sel;
        }
      }
      g_redraw = 1;
      BUZZER_BEEP(20);
    }
    break;

  case key_evt_dn_short:
    /* DN 短按：菜单下移选中 */
    if (g_page == page_menu) {
      if (g_sel < (unsigned char)(menu_max - 1)) {
        g_sel++;
        if (g_sel >= (unsigned char)(g_top + MENU_ROWS)) {
          g_top = (unsigned char)(g_sel - MENU_ROWS + 1);
        }
      }
      g_redraw = 1;
      BUZZER_BEEP(20);
    }
    break;

  case key_evt_up_long:
    /* UP 长按：进入选中的\312\375据页 */
    if (g_page == page_menu) {
      g_page = page_data;
      g_redraw = 1;
      BUZZER_BEEP(50);
    }
    break;

  case key_evt_dn_long:
    /* DN 长按：从\312\375据页返回菜单 */
    if (g_page == page_data) {
      g_page = page_menu;
      g_redraw = 1;
      BUZZER_BEEP(50);
    }
    break;

  default:
    break;
  }
}

/*==================================================================
  对外接口
==================================================================*/
void app_init(void) {
  g_page = page_home;
  g_sel = 0;
  g_top = 0;
  g_redraw = 1;
  g_rtc_ok = 0;
  g_blink = 0;
  g_last_1s = 0;
  g_last_10s = 0;

  bsp_rtc_init();
  bsp_gps_init();
  bsp_bat_init();
  bsp_sens_init();
  bsp_pms_init();
  bsp_modbus_init();
  bsp_wind_init();
  bsp_noise_init();
  bsp_log_init();

  /* 稳定一次电池和 RTC，读到第一个有效值 */
  bsp_bat_update();
  g_rtc_ok = bsp_rtc_read(&g_rtc);
}

void app_run(void) {
  key_evt_t evt;
  unsigned int now;

  /* --- 1. 收 GPS / PMS 粉尘 / RS485 噪声风速，尽量不阻塞地轮询 --- */
  bsp_gps_poll();
  bsp_pms_poll();
  bsp_wind_poll();
  bsp_noise_poll();
  bsp_log_tick();

  /* 调试命令：按 1~9 = 把 485 上的从机(地址1)改成该地址；按 0 = 扫描总线 */
  {
    unsigned char cmd;
    while ((cmd = UART1_GETC()) != 0) {
      if (cmd >= '1' && cmd <= '9') {
        modbus_set_addr((unsigned char)(cmd - '0'));
      } else if (cmd == '0') {
        modbus_scan();
      }
    }
  }

  /* --- 2. 轮流刷新一个传感器 --- */
  bsp_sens_update_next();

  /* --- 3. 有有效 GPS 时间就授时一次 --- */
  if (bsp_gps_sync_rtc()) {
    g_rtc_ok = bsp_rtc_read(&g_rtc);
    g_redraw = 1;
  }

  /* --- 4. 处理按键事件 --- */
  while ((evt = bsp_key_get_event()) != key_evt_none) {
    app_on_key(evt);
  }

  /* --- 5. 定时更新 --- */
  now = g_ms;

  if ((unsigned int)(now - g_last_1s) >= 1000U) {
    g_last_1s = now;
    g_rtc_ok = bsp_rtc_read(&g_rtc); /* 每秒读一次 RTC */
    if (g_page == page_home || g_page == page_data) {
      g_redraw = 1;
    }
    /* 闪烁相位每 500ms 翻一次 */
    if (g_page == page_home) {
      g_blink = (unsigned char)(((now / 500U) & 1U) ? 1 : 0);
    }
  }

  if ((unsigned int)(now - g_last_10s) >= 10000U) {
    g_last_10s = now;
    bsp_bat_update(); /* 电池 10 秒刷一次 */
    if (g_page == page_home || g_page == page_data) {
      g_redraw = 1;
    }
  }

  /* --- 6. 状态指示灯：GPS + TF --- */
  {
    gps_info_t *g = bsp_gps_get();
    unsigned int sv;
    unsigned char bl;
    unsigned char ant_bad;

    bl = (unsigned char)((now / 500U) & 1U);
    sv = (unsigned int)g->sats_used + (unsigned int)g->sats_view;
    ant_bad = (unsigned char)((g->ant == 2U || g->ant == 3U) ? 1 : 0);

    if (ant_bad) {
      LED_GPS(LED_RED); /* 天线断开: 红常亮 */
    } else if (g->lines == 0 || sv == 0U) {
      LED_GPS(bl ? LED_RED : LED_OFF); /* 没星: 红闪 */
    } else if (!g->fix) {
      LED_GPS(bl ? LED_GRN : LED_OFF); /* 搜星中: 绿闪 */
    } else {
      LED_GPS(LED_GRN); /* 稳定定位: 绿常亮 */
    }

    if (bsp_log_ok()) {
      LED_TF(bl ? LED_GRN : LED_OFF); /* 记录中: 绿闪(1s) */
    } else {
      LED_TF(LED_RED); /* 没卡: 红常亮 */
    }
  }

  /* --- 7. 需要就重画 --- */
  if (g_redraw) {
    g_redraw = 0;
    app_draw();
  }
}
