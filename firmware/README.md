# 固件（firmware）

STC8H8K64U 的 Keil C51 工程。源码为 **GBK 编码**。

## 目录

```
APP/        应用层
  main.c        初始化 + 10 ms 主循环（扫按键 → app_run）
  app.c/.h      页面状态机：主页 / 菜单 / 数据页；菜单项与按键事件
BSP/        板级驱动
  tick.c        Timer0 1 ms 系统滴答（g_ms / delay_ms）
  led.c         状态 LED（含三态控制）
  buzzer.c      蜂鸣器
  uart.c        UART1..4 + RS485 方向控制（UART4_SET_BAUD 运行时改波特率）
  adc.c         ADC（电池电压，含平均/统计）
  i2c.c         软件 I²C 主机
  spi.c         硬件 SPI + TF/SD 卡驱动
  oled.c        SSD1306 OLED
  font.c        GT20L16S1Y 中文字库（位翻转 SPI）
  bsp_key.c     按键事件（短按 / 长按）
  bsp_rtc.c     PCF8563 RTC
  bsp_gps.c     GPS NMEA 解析 + 授时
  bsp_sens.c    I²C 传感器：SHT45 / SCD41 / VEML7700 / IST8310（+ BMP581 SPI）
  bsp_bat.c     电池电压/电量
  bsp_ui.c      显示底层：整行文本、柱状条、×10 定点/无符号格式化
  bsp_pms.c     PMS7003 颗粒物（UART3 接收中断 + 环形缓冲）
  bsp_modbus.c  RS485 Modbus-RTU 主机（读/改址/扫描）
  bsp_noise.c   噪声传感器（地址 1）
  bsp_wind.c    风速（地址 2）+ 风向（地址 3）
  bsp_log.c     TF 卡数据记录（FatFs，开机自动写 LOG.CSV）
  city_table.h  341 个地级市经纬度表 + 最近城市查找（GPS 页右侧显示）
FATFS/      FatFs R0.16（ff.c/ff.h/ffconf.h/diskio.c/diskio.h）
MCU/        工程配置
  Config.h      主频、外设配置
  Type_def.h    类型别名
Project/    Keil 工程（project.uvproj）
```

## 编译 / 烧录

- uVision 打开 `Project/project.uvproj`（器件 `STC8H8K64U Series`，寄存器头 `STC8H.H`）。
- 命令行：

  ```powershell
  Start-Process "C:\Keil_v5\UV4\UV4.exe" -ArgumentList '-b','Project\project.uvproj','-o','build.log' -Wait
  ```

- 用 STC-ISP 烧录取 `OUT/project.hex`（无 SWD/JTAG，只能串口 ISP，需冷启动）。

## 约定

- >8 字节缓冲区放 `xdata`（8051 DATA 仅 128 字节）。
- **内存模型为 large**（为容纳 FatFs 而设；变量默认进 xdata）。
- **注意**：GBK 源码中含 `0xFD` 字节的汉字需转义，详见 `../docs/..._踩坑记录.md` §1.1。
