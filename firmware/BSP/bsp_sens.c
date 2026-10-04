#include "bsp_sens.h"
#include "i2c.h"
#include "spi.h"
#include "uart.h"
#include "tick.h"

/* 从机地址 */
#define SENS_ADDR_IST8310 0x0C
#define SENS_ADDR_VEML7700 0x10
#define SENS_ADDR_SHT45 0x44
#define SENS_ADDR_SCD41 0x62

static sens_info_t xdata g_sens;
static unsigned char xdata g_rb[12]; /* 读传感器的临时缓冲 */
static unsigned char g_next;         /* 下一个要刷新的传感器 */

/*==================================================================
  Sensirion 通用 CRC8（多项式 0x31，初值 0xFF）
  SHT45 和 SCD41 都用它校验
==================================================================*/
static unsigned char sens_crc8(unsigned char *buf, unsigned char n) {
    unsigned char crc;
    unsigned char i;
    unsigned char b;

    crc = 0xFF;
    for (i = 0; i < n; i++)
    {
        crc ^= buf[i];
        for (b = 0; b < 8; b++)
        {
            if (crc & 0x80)
            {
                crc = (unsigned char)((crc << 1) ^ 0x31);
            } else
            {
                crc = (unsigned char)(crc << 1);
            }
        }
    }
    return crc;
}

/*==================================================================
  各个传感器的读取
==================================================================*/

/* SHT45：命令 0xFD 高精度测量；T = -45 + 175*t/65535，RH = -6 + 125*rh/65535
   ★这两个偏移很容易漏，漏了温度会虚高 45 度（详见 doc/测试经验总结.md §2.4） */
static void sens_read_sht45(void) {
    unsigned long u;

    if (!I2C_PING(SENS_ADDR_SHT45))
    {
        g_sens.sht_ok = 0;
        return;
    }
    if (!I2C_SEND_CMD(SENS_ADDR_SHT45, 0x00FD))
    {
        g_sens.sht_ok = 0;
        return;
    }
    delay_ms(12); /* 高精度测量最长 8.3ms，给足 12ms */
    if (!I2C_READ_BYTES(SENS_ADDR_SHT45, g_rb, 6))
    {
        g_sens.sht_ok = 0;
        return;
    }
    /* CRC 不对就丢弃这次\xCA\375据 */
    if (sens_crc8(&g_rb[0], 2) != g_rb[2] || sens_crc8(&g_rb[3], 2) != g_rb[5])
    {
        g_sens.sht_ok = 0;
        return;
    }

    u = (unsigned long)(((unsigned int)g_rb[0] << 8) | g_rb[1]);
    g_sens.sht_t10 = (int)(u * 1750UL / 65535UL) - 450;

    u = (unsigned long)(((unsigned int)g_rb[3] << 8) | g_rb[4]);
    {
        int rh10;
        rh10 = (int)(u * 1250UL / 65535UL) - 60;
        if (rh10 < 0)
        {
            rh10 = 0;
        }
        if (rh10 > 1000)
        {
            rh10 = 1000;
        }
        g_sens.sht_rh10 = (unsigned int)rh10;
    }
    g_sens.sht_ok = 1;
}

/* VEML7700：ALS 是"一个 16 位寄存器"（命令码 0x04，低字节先出）。
   ★坑1：命令码 0x00(ALS_CONF) 上电默认 = 0x0001，bit0(ALS_SD)=1 是 shutdown，
         不配置的话 ALS 永远读回 0。必须先写 0x0000 上电。
   ★坑2：不能像两个独立 8 位寄存器那样分别读 0x04/0x05 —— 0x05 是 WHITE
         通道。必须对 0x04 做一次连续读，拿回 低字节+高字节。 */
static void sens_read_veml7700(void) {
    unsigned char b[2];

    if (!I2C_PING(SENS_ADDR_VEML7700))
    {
        g_sens.als_ok = 0;
        return;
    }
    if (!I2C_CMD_READ(SENS_ADDR_VEML7700, 0x04, b, 2))
    {
        g_sens.als_ok = 0;
        return;
    }
    g_sens.als = ((unsigned int)b[1] << 8) | b[0];
    g_sens.als_ok = 1;
}

/* IST8310：X/Y/Z 各 16 位，低字节在前（0x03/0x05/0x07 起）。
   ★坑：上电后是 Stand-By，CNTL1(0x0A)[3:0]=0 时\xCA\375据寄存器一直是 0。
        写 CNTL1=0x01 触发一次 Single Measurement，等 ≥6ms 再读 0x03~0x08；
        每次读完自动回 Stand-By，所以每次都要重新触发。 */
static void sens_read_ist8310(void) {
    unsigned char b[2];

    if (!I2C_PING(SENS_ADDR_IST8310))
    {
        g_sens.mag_ok = 0;
        return;
    }
    /* 触发单次测量 */
    if (!I2C_WRITE_REG(SENS_ADDR_IST8310, 0x0A, 0x01))
    {
        g_sens.mag_ok = 0;
        return;
    }
    delay_ms(10); /* 单次测量最短 5~6ms，给足 10ms */

    if (!I2C_CMD_READ(SENS_ADDR_IST8310, 0x03, b, 2))
    {
        g_sens.mag_ok = 0;
        return;
    }
    g_sens.mag_x = (int)(((unsigned int)b[1] << 8) | b[0]);

    if (!I2C_CMD_READ(SENS_ADDR_IST8310, 0x05, b, 2))
    {
        g_sens.mag_ok = 0;
        return;
    }
    g_sens.mag_y = (int)(((unsigned int)b[1] << 8) | b[0]);

    if (!I2C_CMD_READ(SENS_ADDR_IST8310, 0x07, b, 2))
    {
        g_sens.mag_ok = 0;
        return;
    }
    g_sens.mag_z = (int)(((unsigned int)b[1] << 8) | b[0]);
    g_sens.mag_ok = 1;
}

/*==================================================================
  BMP581 气压 / 海拔
  ------------------------------------------------------------------
  寄存器（手册第 7 章）：
      0x01 CHIP_ID（应为 0x50）      0x02 REV_ID（0x32）
      0x1D~0x1F 温度 XLSB/LSB/MSB    0x20~0x22 压力 XLSB/LSB/MSB
      0x30 DSP_CONFIG   0x36 OSR_CONFIG   0x37 ODR_CONFIG
  \xCA\375据格式（手册 7.11~7.16）：
      温度 (signed, 24, 16) [°C]  ->  T(°C) = raw / 65536
      压力 (signed, 24, 6)  [Pa]  ->  P(Pa) = raw / 64
  ★关键：BMP581 片内有 DSP 可以直接做系\xCA\375补偿（DSP_CONFIG.comp_pt_en），
    所以【不用】在 8051 上算那套 64 位补偿公式（C51 也没有 64 位整\xCA\375）。
    把 comp_pt_en 设成 0b11，读回来的就是补偿好的值。

  海拔换算：用 ISA 标准大气表（每 500 米一个点）做线性插值，
    比"1hPa = 8.4 米"的线性近似准得多，而且不用浮点。
==================================================================*/

/* ISA 标准大气：0/500/1000/1500/2000 米处的气压（hPa × 10） */
static const unsigned int code g_alt_p[5] = {10132, 9546, 8988, 8456, 7949};

/* 气压(Pa) -> 海拔(m)。超出 0~2000m 范围时按端点外推 */
static int bmp_pa_to_alt(unsigned long pa) {
    unsigned int p10; /* hPa × 10 */
    unsigned long num;
    unsigned long den;
    unsigned char i;

    p10 = (unsigned int)(pa / 10UL); /* Pa -> hPa×10 */

    i = 0;
    while (i < 4 && p10 < g_alt_p[i + 1])
    {
        i++;
    }

    den = (unsigned long)(g_alt_p[i] - g_alt_p[i + 1]);
    if (p10 >= g_alt_p[i])
    {
        /* 比本段起点还高（气压更低）-> 往上外推 */
        num = (unsigned long)(p10 - g_alt_p[i]) * 500UL;
        return (int)((unsigned int)(i * 500U) - (unsigned int)(num / den));
    }
    num = (unsigned long)(g_alt_p[i] - p10) * 500UL;
    return (int)((unsigned int)(i * 500U) + (unsigned int)(num / den));
}

static unsigned char g_bmp_cfg_done;

static void sens_read_bmp581(void) {
    unsigned char b[6];
    unsigned long t_raw;
    unsigned long p_raw;
    unsigned long pa;

    if (BMP_READ_REG(0x01) != 0x50)
    {
        g_sens.bmp_ok = 0;
        return;
    }

    /* --- 第一次读之前先配置 --- */
    if (!g_bmp_cfg_done)
    {
        /* OSR_CONFIG：press_en=1(0x40) + osr_p=010(4x -> 0x10) + osr_t=000(1x) */
        BMP_WRITE_REG(0x36, 0x50);
        /* DSP_CONFIG：comp_pt_en = 0b11，压力和温度都用片内补偿 */
        BMP_WRITE_REG(0x30, 0x03);
        g_bmp_cfg_done = 1;
    }

    /* --- 触发一次强制测量：ODR_CONFIG 的 pwr_mode = 0b10 ---
       ODR_CONFIG：bit7=deep_dis, bit6:2=odr, bit1:0=pwr_mode
       这里 deep_dis=1 关掉深度\xB4\375机，odr 随便，pwr_mode=10(forced) */
    BMP_WRITE_REG(0x36, 0x50);
    BMP_WRITE_REG(0x30, 0x03);
    BMP_WRITE_REG(0x37, 0x82);
    delay_ms(50); /* 等一次转换完成（4x \xB9\375采样约几毫秒） */

    /* --- 读 0x1D..0x22：温度 3 字节 + 压力 3 字节 --- */
    BMP_READ_BURST(0x1D, b, 6);

    /* 温度：24 位有符号，小\xCA\375点在 bit16 */
    t_raw = ((unsigned long)b[2] << 16) | ((unsigned long)b[1] << 8) | b[0];
    if (t_raw & 0x800000UL)
    {
        t_raw = t_raw - 0x1000000UL; /* 24 位符号扩展 */
    }

    /* 压力：24 位有符号，小\xCA\375点在 bit6 -> Pa */
    p_raw = ((unsigned long)b[5] << 16) | ((unsigned long)b[4] << 8) | b[3];
    if (p_raw & 0x800000UL)
    {
        p_raw = p_raw - 0x1000000UL;
    }
    pa = p_raw / 64UL;

    if (pa < 30000UL || pa > 110000UL)
    {
        g_sens.bmp_ok = 0; /* 明显不合理，判为读失败 */
        return;
    }

    g_sens.bmp_pa = pa;
    g_sens.bmp_alt_m = bmp_pa_to_alt(pa);
    g_sens.bmp_ok = 1;
}

/* SCD41：0xEC05 读测量（CO2 + T + RH，各带 CRC）
   ★必须先 start_periodic_measurement，否则永远 NACK */
static void sens_read_scd41(void) {
    unsigned long u;
    unsigned int w;

    if (!I2C_PING(SENS_ADDR_SCD41))
    {
        g_sens.co2_ok = 0;
        return;
    }
    if (!I2C_SEND_CMD(SENS_ADDR_SCD41, 0xEC05))
    {
        g_sens.co2_ok = 0;
        return;
    }
    delay_ms(2); /* 手册：命令执行时间 1ms */
    if (!I2C_READ_BYTES(SENS_ADDR_SCD41, g_rb, 9))
    {
        /* 5 秒才更新一次，没新\xCA\375据就 NACK —— \xD5\375常，保留上次的值 */
        return;
    }
    if (sens_crc8(&g_rb[0], 2) != g_rb[2] || sens_crc8(&g_rb[3], 2) != g_rb[5] ||
        sens_crc8(&g_rb[6], 2) != g_rb[8])
    {
        return; /* CRC 错，丢弃 */
    }

    g_sens.co2_ppm = ((unsigned int)g_rb[0] << 8) | g_rb[1];
    u = (unsigned long)(((unsigned int)g_rb[3] << 8) | g_rb[4]);
    g_sens.co2_t10 = (int)(u * 1750UL / 65536UL) - 450;
    w = ((unsigned int)g_rb[6] << 8) | g_rb[7];
    g_sens.co2_rh10 = (unsigned int)((unsigned long)w * 1000UL / 65536UL);
    g_sens.co2_ok = 1;
}

/*==================================================================
  对外接口
==================================================================*/

void bsp_sens_init(void) {
    g_sens.sht_ok = 0;
    g_sens.sht_t10 = 0;
    g_sens.sht_rh10 = 0;
    g_sens.bmp_ok = 0;
    g_sens.bmp_pa = 0;
    g_sens.bmp_alt_m = 0;
    g_sens.als_ok = 0;
    g_sens.als = 0;
    g_sens.mag_ok = 0;
    g_sens.mag_x = 0;
    g_sens.mag_y = 0;
    g_sens.mag_z = 0;
    g_sens.co2_ok = 0;
    g_sens.co2_ppm = 0;
    g_sens.co2_t10 = 0;
    g_sens.co2_rh10 = 0;
    g_next = 0;

    /* VEML7700：ALS_CONF(0x00) 上电默认 shutdown，写 0x0000 上电开测
       （gain×1、\xBB\375分时间 100ms），等一个\xBB\375分周期再读 */
    I2C_WRITE_REG16(SENS_ADDR_VEML7700, 0x00, 0x0000);
    delay_ms(120);

    /* IST8310：手册推荐的初始设置（平均次\xCA\375 AVGCNTL / set-reset 脉冲 PDCNTL） */
    I2C_WRITE_REG(SENS_ADDR_IST8310, 0x41, 0x24);
    I2C_WRITE_REG(SENS_ADDR_IST8310, 0x42, 0xC0);

    /* SCD41 上电后是 idle，必须先启动周期测量（信号 5 秒更新一次）。
     先 stop 一次保证幂等，再 start */
    I2C_SEND_CMD(SENS_ADDR_SCD41, 0x3F86);
    delay_ms(600);
    I2C_SEND_CMD(SENS_ADDR_SCD41, 0x21B1);
}

/* 轮流刷新一个传感器，主循环每圈调一次 */
void bsp_sens_update_next(void) {
    switch ((sens_id_t)g_next)
    {
    case sens_id_sht45:
        sens_read_sht45();
        break;
    case sens_id_bmp581:
        sens_read_bmp581();
        break;
    case sens_id_veml7700:
        sens_read_veml7700();
        break;
    case sens_id_ist8310:
        sens_read_ist8310();
        break;
    case sens_id_scd41:
        sens_read_scd41();
        break;
    default:
        break;
    }

    g_next++;
    if (g_next >= (unsigned char)sens_id_max)
    {
        g_next = 0;
    }
}

sens_info_t *bsp_sens_get(void) {
    return &g_sens;
}
