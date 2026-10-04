#include "Config.h"
#include "app.h"
#include "bsp_key.h"
#include "buzzer.h"
#include "font.h"
#include "i2c.h"
#include "led.h"
#include "oled.h"
#include "spi.h"
#include "tick.h"
#include "uart.h"

/*==================================================================
  环境监测仪 —— 主程序
  ------------------------------------------------------------------
  main() 只做两件事：
      1. 把所有模块初始化一遍
      2. 一个固定节拍的主循环：扫按键 -> 跑应用
  具体业务（页面、菜单、\xCA\375据刷新）全在 app.c 里。
==================================================================*/

/* 主循环节拍（ms）。按键扫描和它绑定，bsp_key_scan 里用 g_ms 计时，
   所以即使某一圈被传感器读取拖长了，长按判定也不会跑偏 */
#define MAIN_TICK_MS 10

void main(void) {
    P_SW2 |= 0x80; /* EAXFR = 1：之后才能访问扩展 SFR（SPI/ADC/IAP 等） */

    /* ---------------- 初始化 ---------------- */
    TICK_INIT();  /* 1ms 系统滴答（Timer0） */
    UART1_INIT(); /* 调试串口，115200 */
    LED_INIT();
    BUZZER_INIT();
    SPI_INIT(); /* SPI 总线：BMP581 / TF 卡 / OLED / 字库共用 */
    OLED_INIT();
    FONT_INIT(); /* 字库芯片 */
    I2C_INIT();  /* 5 个 I2C 传感器共用 */
    bsp_key_init();

    /* 应用层：初始化各个\xCA\375据模块 + 画第一屏 */
    app_init();

    /* ---------------- 上电提示 ---------------- */
    BUZZER_BEEP(60);
    LED_TF(LED_GRN);
    delay_ms(200);
    LED_TF(LED_OFF);


    UART1_PUTS("=== 环境监测仪 ===\r\n");

    /* ---------------- 主循环 ---------------- */
    while (1)
    {
        bsp_key_scan(); /* 按键扫描（内部按 g_ms 计时，周期不严格也没关系） */
        app_run();      /* 收 GPS、刷传感器、处理事件、重画界面 */
        delay_ms(MAIN_TICK_MS);
    }
}
