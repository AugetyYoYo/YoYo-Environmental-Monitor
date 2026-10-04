#include "bsp_rtc.h"
#include "i2c.h"

/* PCF8563 的 7 位 I2C 地址 */
#define RTC_ADDR 0x51

/* 寄存器编号 */
#define RTC_REG_CTRL1 0x00
#define RTC_REG_SEC 0x02
#define RTC_REG_MIN 0x03
#define RTC_REG_HOUR 0x04
#define RTC_REG_DAY 0x05
#define RTC_REG_WEEK 0x06
#define RTC_REG_MONTH 0x07
#define RTC_REG_YEAR 0x08

/* 十进制 -> BCD */
static unsigned char rtc_dec2bcd(unsigned char v) {
    return (unsigned char)(((v / 10) << 4) | (v % 10));
}

/* BCD -> 十进制 */
static unsigned char rtc_bcd2dec(unsigned char v) {
    return (unsigned char)(((v >> 4) * 10) + (v & 0x0F));
}

/* 初始化：控制寄存器 1/2 归零（\xD5\375常运行、不测试、不停止晶振） */
void bsp_rtc_init(void) {
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_CTRL1, 0x00);
    I2C_WRITE_REG(RTC_ADDR, 0x01, 0x00);
}

/* 读一个寄存器；失败返回 0xFF */
static unsigned char rtc_read_reg(unsigned char reg) {
    unsigned char v;

    if (I2C_READ_REG(RTC_ADDR, reg, &v) == 0)
    {
        return 0xFF;
    }
    return v;
}

unsigned char bsp_rtc_read(rtc_time_t *t) {
    unsigned char v;

    v = rtc_read_reg(RTC_REG_SEC);
    if (v == 0xFF)
    {
        return 0;
    }
    t->valid = (unsigned char)((v & 0x80) ? 0 : 1); /* bit7 = VL */
    t->second = rtc_bcd2dec((unsigned char)(v & 0x7F));

    t->minute = rtc_bcd2dec((unsigned char)(rtc_read_reg(RTC_REG_MIN) & 0x7F));
    /* 0x04 的 bit6 = 0 表示 24 小时制，bit5 是保留位 */
    t->hour = rtc_bcd2dec((unsigned char)(rtc_read_reg(RTC_REG_HOUR) & 0x3F));
    t->day = rtc_bcd2dec((unsigned char)(rtc_read_reg(RTC_REG_DAY) & 0x3F));
    t->weekday = (unsigned char)(rtc_read_reg(RTC_REG_WEEK) & 0x07);
    /* 0x07 的 bit7 是世纪位，这里只取低 5 位的月 */
    t->month = rtc_bcd2dec((unsigned char)(rtc_read_reg(RTC_REG_MONTH) & 0x1F));
    t->year = rtc_bcd2dec(rtc_read_reg(RTC_REG_YEAR));

    return 1;
}

/* 星期：0=周日 ... 6=周六，Sakamoto 算法（不需要查表） */
unsigned char bsp_rtc_weekday(unsigned char year, unsigned char month, unsigned char day) {
    static const unsigned char code t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    unsigned int y;

    y = (unsigned int)(2000U + year);
    if (month < 3)
    {
        y = y - 1U;
    }
    return (unsigned char)((y + y / 4U - y / 100U + y / 400U + t[month - 1] + day) % 7U);
}

void bsp_rtc_write(rtc_time_t *t) {
    /* 0x00 控制寄存器归零；0x02 秒（同时把 VL 清 0） */
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_CTRL1, 0x00);
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_SEC, rtc_dec2bcd(t->second));
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_MIN, rtc_dec2bcd(t->minute));
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_HOUR, rtc_dec2bcd(t->hour)); /* bit6=0 -> 24h */
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_DAY, rtc_dec2bcd(t->day));
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_WEEK, bsp_rtc_weekday(t->year, t->month, t->day));
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_MONTH, rtc_dec2bcd(t->month)); /* bit7=0 -> 20xx */
    I2C_WRITE_REG(RTC_ADDR, RTC_REG_YEAR, rtc_dec2bcd(t->year));
}
