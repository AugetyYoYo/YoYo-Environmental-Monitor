#include "bsp_key.h"
#include "tick.h"

/*==================================================================
  内部\xCA\375据结构
==================================================================*/

/* 每个按键一个状态机 */
typedef struct {
    unsigned char raw_last;  /* 上次读到的原始电平（1 = 按下） */
    unsigned char stable;    /* 消抖后的稳定电平 */
    unsigned char cnt;       /* 连续读到相同电平的次\xCA\375 */
    unsigned int hold;       /* 已经按住了多少毫秒 */
    unsigned char long_sent; /* 长按事件是否已经发出\xB9\375 */
} key_chan_t;

/* 状态放 xdata：STC8H 的 DATA 只有 128 字节，很宝贵 */
static key_chan_t xdata g_key[key_id_max];

/* 事件队列：4 格环形缓冲，防止连续按键时丢事件 */
static unsigned char xdata g_evt_q[4];
static unsigned char g_evt_w; /* 写指针 */
static unsigned char g_evt_r; /* 读指针 */

/*==================================================================
  内部函\xCA\375
==================================================================*/

/* 读一个键的物理状态：返回 1 = 按下，0 = 松开 */
static unsigned char key_read_pin(key_id_t id) {
    switch (id)
    {
    case key_id_mode:
        return (key_mode_pin == 0) ? 1 : 0;
    case key_id_up:
        return (key_up_pin == 0) ? 1 : 0;
    case key_id_dn:
        return (key_dn_pin == 0) ? 1 : 0;
    default:
        return 0;
    }
}

/* 把事件放进队列；队列满了就丢弃（\xD5\375常情况下不会满） */
static void key_push_event(key_evt_t evt) {
    unsigned char next;

    next = (unsigned char)((g_evt_w + 1) & 0x03);
    if (next == g_evt_r)
    {
        return;
    }
    g_evt_q[g_evt_w] = (unsigned char)evt;
    g_evt_w = next;
}

/* 按键编号 -> 短按事件号 */
static key_evt_t key_short_event(key_id_t id) {
    if (id == key_id_mode)
    {
        return key_evt_mode_short;
    }
    if (id == key_id_up)
    {
        return key_evt_up_short;
    }
    return key_evt_dn_short;
}

/* 按键编号 -> 长按事件号 */
static key_evt_t key_long_event(key_id_t id) {
    if (id == key_id_mode)
    {
        return key_evt_mode_long;
    }
    if (id == key_id_up)
    {
        return key_evt_up_long;
    }
    return key_evt_dn_long;
}

/*==================================================================
  对外接口
==================================================================*/

/* 初始化：\xD2\375脚设为准双向（内部弱上拉），状态清零 */
void bsp_key_init(void) {
    unsigned char i;

    /* PxM1:PxM0 = 0:0 -> 准双向，自带弱上拉；按键按下把\xD2\375脚拉到地 */
    P2M1 &= ~0x80;
    P2M0 &= ~0x80; /* P2.7 MODE */
    P4M1 &= ~0x40;
    P4M0 &= ~0x40; /* P4.6 UP   */
    P0M1 &= ~0x01;
    P0M0 &= ~0x01; /* P0.0 DN   */

    for (i = 0; i < (unsigned char)key_id_max; i++)
    {
        g_key[i].raw_last = 0;
        g_key[i].stable = 0;
        g_key[i].cnt = 0;
        g_key[i].hold = 0;
        g_key[i].long_sent = 0;
    }
    g_evt_w = 0;
    g_evt_r = 0;
}

/* 周期扫描：每个键跑一遍"消抖 -> 边沿检测 -> 长按计时" */
static unsigned int g_key_dt = KEY_SCAN_PERIOD_MS;

/* 用 g_ms 算本次扫描和上次的真实间隔（上限 200ms）。
   主循环某一圈可能被传感器读取拖长，如果按固定周期累加，
   长按 1 秒的判定就会跑偏，所以必须用真实时间 */
static void key_update_dt(void) {
    static unsigned int last_ms;
    unsigned int now_ms;
    unsigned int dt;

    now_ms = g_ms;
    dt = (unsigned int)(now_ms - last_ms);
    last_ms = now_ms;
    if (dt > 200U)
    {
        dt = 200U;
    }
    g_key_dt = dt;
}

void bsp_key_scan(void) {
    unsigned char i;
    unsigned char now;

    key_update_dt();

    for (i = 0; i < (unsigned char)key_id_max; i++)
    {
        now = key_read_pin((key_id_t)i);

        /* ---- 第 1 步：消抖 ---- */
        if (now != g_key[i].raw_last)
        {
            /* 电平刚变化，重新计\xCA\375，等下一个节拍再判 */
            g_key[i].raw_last = now;
            g_key[i].cnt = 0;
            continue;
        }
        if (g_key[i].cnt < KEY_DEBOUNCE_TICKS)
        {
            g_key[i].cnt++;
            if (g_key[i].cnt < KEY_DEBOUNCE_TICKS)
            {
                continue; /* 还没连续稳定够次\xCA\375 */
            }
        }

        /* ---- 第 2 步：稳定电平发生变化 -> 是"按下"或"松开" ---- */
        if (now != g_key[i].stable)
        {
            g_key[i].stable = now;
            if (now == 1)
            {
                /* 刚按下：重新开始计时 */
                g_key[i].hold = 0;
                g_key[i].long_sent = 0;
            } else
            {
                /* 刚松开：如果从头到尾没发\xB9\375长按，那它就是一个短按 */
                if (g_key[i].long_sent == 0)
                {
                    key_push_event(key_short_event((key_id_t)i));
                }
            }
        }

        /* ---- 第 3 步：按住期间累计时长，够 1 秒就发长按事件 ---- */
        if (g_key[i].stable == 1)
        {
            if (g_key[i].hold < 0xFFFFU)
            {
                g_key[i].hold = (unsigned int)(g_key[i].hold + g_key_dt);
            }
            if (g_key[i].long_sent == 0 && g_key[i].hold >= KEY_LONG_MS)
            {
                g_key[i].long_sent = 1;
                key_push_event(key_long_event((key_id_t)i));
            }
        }
    }
}

/* 取一个事件；没有就返回 key_evt_none */
key_evt_t bsp_key_get_event(void) {
    key_evt_t evt;

    if (g_evt_r == g_evt_w)
    {
        return key_evt_none;
    }
    evt = (key_evt_t)g_evt_q[g_evt_r];
    g_evt_r = (unsigned char)((g_evt_r + 1) & 0x03);
    return evt;
}
