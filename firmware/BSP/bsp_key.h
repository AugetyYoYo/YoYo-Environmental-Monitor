#ifndef _BSP_KEY_H
#define _BSP_KEY_H

#include "STC8H.H"

/*==================================================================
  按键事件驱动（3 个按键：MODE / UP / DN）
  ------------------------------------------------------------------
  硬件（硬件文档 4.5 节）：三个键都是"按下接地"，低电平有效
      key_mode_pin = P2.7
      key_up_pin   = P4.6
      key_dn_pin   = P0.0

  为什么要单独做一个"事件层"：
      裸扫描只能回答"现在有没有按下"，做不了"按住 1 秒"这种判断。
      事件层给每个键维护一个小状态机（消抖 -> 按下 -> 计时 -> 松开），
      把物理动作翻译成"短按 / 长按"两种事件，业务层就完全不用管时序。

  用法：
      1. 初始化          bsp_key_init();
      2. 每 10ms 调一次  bsp_key_scan();       <- 放进定时器中断或主循环节拍
      3. 主循环里取事件  bsp_key_get_event();  <- 没有事件时返回 key_evt_none
==================================================================*/

/* 扫描周期（ms）—— 必须和实际调用 bsp_key_scan() 的间隔一致 */
#define KEY_SCAN_PERIOD_MS 10

/* 按住多久算长按（ms） */
#define KEY_LONG_MS 1000

/* 消抖：连续读到相同电平几次才算稳定（2 次 = 20ms） */
#define KEY_DEBOUNCE_TICKS 2

/* 按键编号 */
typedef enum { key_id_mode = 0, key_id_up, key_id_dn, key_id_max } key_id_t;

/* 按键事件 */
typedef enum {
    key_evt_none = 0,   /* 没有事件 */
    key_evt_mode_short, /* MODE 短按 */
    key_evt_mode_long,  /* MODE 长按 */
    key_evt_up_short,   /* UP   短按 */
    key_evt_up_long,    /* UP   长按 */
    key_evt_dn_short,   /* DN   短按 */
    key_evt_dn_long     /* DN   长按 */
} key_evt_t;

/* 三个按键的引脚（按下 = 低电平） */
sbit key_mode_pin = P2 ^ 7;
sbit key_up_pin = P4 ^ 6;
sbit key_dn_pin = P0 ^ 0;

/* 初始化引脚（准双向模式，内部弱上拉） */
void bsp_key_init(void);

/* 周期扫描：每 KEY_SCAN_PERIOD_MS 毫秒调用一次 */
void bsp_key_scan(void);

/* 取一个按键事件；队列为空返回 key_evt_none */
key_evt_t bsp_key_get_event(void);

#endif
