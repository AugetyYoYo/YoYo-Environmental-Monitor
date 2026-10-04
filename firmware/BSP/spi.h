#ifndef __spi_h__
#define __spi_h__

#include "STC8H.H"

/*==================================================================
  SPI 总线 + TF 卡驱动（STC8H 硬件 SPI，主机）
  ------------------------------------------------------------------
    MOSI = P1.3     MISO = P1.4     SCLK = P1.5
  从机片选：
    BMP581 CS = P4.0
    TF  卡 CS = P5.2   <- 本工程唯一参与通信的从机
    OLED   CS = P0.5
    字库   CS = P5.3
  TF 卡检测：TF_CD = P0.4（经 R26=10k 接卡座 CD）
      ★实测极性：插卡 = 0，不插卡 = 1

  ★SPR 档位实测（24MHz，2000 字节，含 CPU 轮询开销）：
      SPR=0 -> 6ms   可用（对应手册 /4）
      SPR=1 -> 8ms   可用（/8）
      SPR=2 -> 13ms  可用（/16，最慢）
      SPR=3 -> 3ms   不可用（比 SPR=0 还快，卡初始化不了）
    所以：初始化用 SPR=2（尽量慢），数据传输用 SPR=0。
    注意 BMP581 当时用 SPR=3 能跑（它支持 10MHz），但 SD 卡不行。

  本工程只调 TF 卡：其它片选全部【推挽输出 + 拉高】，
  目的是排除别的器件被误选中、抢 MISO 的干扰。
==================================================================*/

sbit BMP_CS = P4 ^ 0;
sbit TF_CS = P5 ^ 2;
sbit TF_CD = P0 ^ 4;
/* 注意：OLED_CS / OLED_DC 在 oled.h 里声明，ROM_CS 在 font.h 里声明，
   这里不能再声明一遍，否则 Keil 报重定义。spi.c 里用寄存器直接操作。 */

/* SPI 模式（SD 卡只认 mode0） */
#define SPI_MODE0 0 /* CPOL=0, CPHA=0 */
#define SPI_MODE3 1 /* CPOL=1, CPHA=1 */

/* SPCTL 的 SPR[1:0] 四档（倍率见上面的实测表） */
#define SPI_SPR0 0
#define SPI_SPR1 1
#define SPI_SPR2 2
#define SPI_SPR3 3

/* 初始化：引脚方向 + 全部从机取消选中 + mode0 + SPR0 */
void SPI_INIT(void);

/* 设置 SPI 模式和 SPR 档位 */
void SPI_SET_MODE(unsigned char mode, unsigned char spr);

/* 收发 1 字节，超时返回 0xFF */
unsigned char SPI_XFER(unsigned char d);

/* 连续发 n 个 0xFF（产生时钟） */
void SPI_CLOCKS(unsigned char n);

/* 粗测当前 SPI 时钟：连发 2000 字节，返回耗时(ms)，kHz = 16000/ms */
unsigned int SPI_CLOCK_MS(void);

/*==================================================================
  BMP581（气压计，4 线 SPI）
  ------------------------------------------------------------------
  ★手册 5.5.3：SPI 读【不需要 dummy 字节】，第一字节就是 addr 的内容
  ★寄存器：0x00 = 保留(读回 0x00)，0x01 = CHIP_ID = 0x50，0x02 = REV_ID = 0x32
  CMD 字节 = bit7=R/W(读=1) | bit6:0=地址
==================================================================*/

/* 读一个寄存器 */
void BMP_WRITE_REG(unsigned char reg, unsigned char val);

unsigned char BMP_READ_REG(unsigned char reg);

/* burst 读：从 reg 起连读 n 字节（地址自动递增） */
void BMP_READ_BURST(unsigned char reg, unsigned char *buf, unsigned char n);

/*==================================================================
  TF 卡驱动
==================================================================*/

/* 容量（扇区数，1 扇区 = 512 字节）与卡类型，TF_INIT 成功后有效 */
extern unsigned long g_tf_sectors;
extern unsigned char g_tf_sdhc; /* 1 = SDHC/SDXC(块寻址)，0 = SDSC(字节寻址) */

/* 初始化（含慢速初始化 -> 切快速传输）
   返回 0 = 成功
        1 = 没插卡（CD 高）
        2 = CMD0 无应答
        4 = ACMD41 超时
        5 = OCR / CSD 读取失败 */
unsigned char TF_INIT(void);

/* 读一个扇区（lba 从 0 开始）。返回 0 = 成功 */
unsigned char TF_READ_SECTOR(unsigned long lba, unsigned char xdata *buf);

/* 写一个扇区。返回 0 = 成功，1 = 命令失败，2 = 卡拒收，3 = 写超时 */
unsigned char TF_WRITE_SECTOR(unsigned long lba, unsigned char xdata *buf);

/*---------------- 调试用（诊断隔离） ----------------*/

/* 卡检测脚 P0.4 的原始电平（0 = 插卡，1 = 没卡） */
unsigned char TF_CD_LEVEL(void);

/* 原始 CMD0：CS 高 80 时钟 -> 发 CMD0 -> 连读 n 个字节到 buf */
void TF_CMD0_RAW(unsigned char *buf, unsigned char n);

/* 发一条 SD 命令并等 R1；extra = R1 之后还要读的字节数
   返回 1 = 收到 R1（bit7=0）；buf[0]=R1，buf[1..extra]=后续字节 */
unsigned char TF_CMD_R(unsigned char cmd, unsigned long arg, unsigned char crc, unsigned char ncr,
                       unsigned char *buf, unsigned char extra);

/* 读 16 字节寄存器块（CMD9 = CSD，CMD10 = CID），成功返回 1 */
unsigned char TF_READ_REG16(unsigned char cmd, unsigned char *buf);

#endif
