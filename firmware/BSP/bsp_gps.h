#ifndef _BSP_GPS_H
#define _BSP_GPS_H

#include "STC8H.H"

/*==================================================================
  GPS 模块（ATGM336H，走 UART2，默认 115200-8-N-1）
  ------------------------------------------------------------------
  ★ 用【UART2 接收中断 + 环形缓冲】收数据。
    UART2 只有 1 字节硬件缓冲，而主循环一圈要 10~150ms，115200 下一个
    NMEA 句子 0.6ms 就发完 —— 靠主循环轮询根本收不全，必须用中断。

  只解析 4 类 NMEA 句子，字段按逗号分割，第 0 个字段是句子头：

    $GxRMC,hhmmss.ss,A,lat,N,lon,E,spd,cog,ddmmyy,...
       字段 1=UTC 时间，2=A(有效)/V(无效)，3=纬度，4=N/S，5=经度，6=E/W，9=日期
       判"有没有定位"最可靠是 RMC 的 A（有些模块不发 GGA）

    $GxGGA,... 字段 6=定位质量，字段 7=参与定位卫星数
    $GxGSV,... 字段 3=本句内可见卫星数（多条按星座累加）
    $GxZDA,... 字段 1=UTC，2=日，3=月，4=年
    $GPTXT,... 里带天线状态 OK/OPEN/SHORT

  纬/经度用"整数度 + 小数×10000"存，显示成如 N39.9075 / E116.4074。
==================================================================*/

typedef struct {
    unsigned char fix;       /* 1 = 有效定位（RMC 是 A 或 GGA 质量 0） */
    unsigned char sats_used; /* 参与定位卫星数（来自 GGA） */
    unsigned char sats_view; /* 可见卫星数（GSV 按星座累加） */
    unsigned char year;      /* UTC 年 0~99 */
    unsigned char month;     /* 月 1~12 */
    unsigned char day;       /* 日 1~31 */
    unsigned char hour;      /* 时 0~23（UTC） */
    unsigned char minute;
    unsigned char second;
    unsigned char valid;      /* 1 = 收到过有效 UTC 时间 */
    unsigned char rtc_synced; /* 1 = 已给 RTC 授时 */
    unsigned int lines;       /* 累计收到的 NMEA 句子数（诊断） */
    unsigned char ant;        /* 天线状态：0=未知 1=OK 2=OPEN 3=SHORT */
    unsigned char lat_ns;     /* 纬度半球：'N' / 'S'（0=还没收到） */
    unsigned char lon_ew;     /* 经度半球：'E' / 'W' */
    unsigned int lat_d;       /* 纬度整数度 0~90 */
    unsigned int lat_f4;      /* 纬度小数 ×10000（9075 = .9075） */
    unsigned int lon_d;       /* 经度整数度 0~180 */
    unsigned int lon_f4;      /* 经度小数 ×10000 */
} gps_info_t;

void bsp_gps_init(void);
void bsp_gps_poll(void);
gps_info_t *bsp_gps_get(void);
unsigned char bsp_gps_sync_rtc(void);

#endif
