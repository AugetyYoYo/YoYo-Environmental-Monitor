#ifndef _BSP_UI_H
#define _BSP_UI_H

#include "STC8H.H"

/*==================================================================
  OLED 界面辅助
  ------------------------------------------------------------------
  OLED 是 128×64，按 8 像素一"页"，共 8 页。字库是 8×16 的 ASCII
  和 16×16 的汉字，所以一行文字占 2 页，一共 4 行：
      第 0 行 -> page 0
      第 1 行 -> page 2
      第 2 行 -> page 4
      第 3 行 -> page 6
  一行 128 像素 = 16 个 ASCII 字符（或 8 个汉字）。

  这里只放"画界面用的通用工具"，具体每个页面长什么样在 app.c 里。
==================================================================*/

/* 一行文字最多 16 个字符位（ASCII 算 1，汉字算 2） */
#define UI_LINE_CHARS 16

/* 清屏 */
void bsp_ui_clear(void);

/* 在第 page 页显示一行文字，右边自动补空格（把上一屏的残留擦掉）
   参数是 GB2312 字符串，汉字能直接显示（字库芯片里有） */
void bsp_ui_line(unsigned char page, char *s);

/* 电量竖条：在 page 页、col 列开始画 4 条竖条，bars = 亮几条（0~4）
   blink = 1 时整条闪烁（电量低时用） */
void bsp_ui_bars(unsigned char page, unsigned char col, unsigned char bars, unsigned char blink);

/* 把 ×10 的定点数格式化成字符串，如 272 -> "27.2"、-45 -> "-4.5"、
   591 -> "59.1"。buf 至少要 7 字节。返回 buf */
char *bsp_ui_fmt10(int v);

/* 把 0~65535 格式化成字符串（不补前导零）。buf 至少要 6 字节。返回 buf */
char *bsp_ui_fmtu(unsigned int v);

#endif
