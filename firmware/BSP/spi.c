#include "spi.h"
#include "Config.h"
#include "tick.h"

#define SPI_TIMEOUT 5000U

/* 初始化用最\xC2\375的一档，\xCA\375据传输用最快的一档（实测表见 spi.h） */
#define TF_SPR_INIT SPI_SPR2
#define TF_SPR_XFER SPI_SPR0

unsigned long g_tf_sectors;
unsigned char g_tf_sdhc;

/* 内部小缓冲（放 xdata，省 DATA 空间） */
static unsigned char xdata sd_rb[16];

/* 其它从机的片选：故意用和 oled.h / font.h 不同的名字，
   这样在 spi.c 里用 sbit 单独操作，不会像 P0 |= xx 那样连带写别的\xD2\375脚 */
sbit SPI_OLED_CS = P0 ^ 5;
sbit SPI_OLED_DC = P0 ^ 6;
sbit SPI_ROM_CS = P5 ^ 3;

/*==================================================================
  SPI 初始化
  ------------------------------------------------------------------
  MOSI(P1.3) / SCLK(P1.5) 推挽输出，MISO(P1.4) 高阻输入
  所有从机 CS 全部配成推挽输出并拉高，只让 TF_CS 参与通信
==================================================================*/
void SPI_INIT(void) {
    /* MOSI = P1.3 推挽 */
    P1M1 &= ~0x08;
    P1M0 |= 0x08;
    /* SCLK = P1.5 推挽 */
    P1M1 &= ~0x20;
    P1M0 |= 0x20;
    /* MISO = P1.4 高阻输入 */
    P1M1 |= 0x10;
    P1M0 &= ~0x10;

    /* 片选脚：全部推挽输出 */
    P4M1 &= ~0x01;
    P4M0 |= 0x01; /* BMP_CS  P4.0 */
    P5M1 &= ~0x04;
    P5M0 |= 0x04; /* TF_CS   P5.2 */
    P0M1 &= ~0x20;
    P0M0 |= 0x20; /* OLED_CS P0.5 */
    P0M1 &= ~0x40;
    P0M0 |= 0x40; /* OLED_DC P0.6 */
    P5M1 &= ~0x08;
    P5M0 |= 0x08; /* ROM_CS  P5.3 */

    /* 先全部取消选中 */
    BMP_CS = 1;
    TF_CS = 1;
    SPI_OLED_CS = 1;
    SPI_ROM_CS = 1;
    SPI_OLED_DC = 0;

    /* TF 卡检测脚 P0.4：准双向（内部弱上拉） */
    P0M1 &= ~0x10;
    P0M0 &= ~0x10;

    /* SPI \xD2\375脚 = P1.3/P1.4/P1.5：P_SW1 bit3:2 = 00 */
    P_SW1 &= ~0x0C;

    SPI_SET_MODE(SPI_MODE0, SPI_SPR0);
}

/*==================================================================
  设置 SPI 模式和 SPR 档位
  ------------------------------------------------------------------
  SPCTL: SSIG(7) SPEN(6) DORD(5) MSTR(4) CPOL(3) CPHA(2) SPR(1:0)
==================================================================*/
void SPI_SET_MODE(unsigned char mode, unsigned char spr) {
    unsigned char ctl;

    ctl = SSIG | SPEN | MSTR | (unsigned char)(spr & 0x03);
    if (mode == SPI_MODE3)
    {
        ctl |= CPOL | CPHA;
    }
    SPCTL = ctl;
    SPSTAT = SPIF | WCOL;
}

/*==================================================================
  收发 1 字节（超时返回 0xFF，不会死等）
==================================================================*/
unsigned char SPI_XFER(unsigned char d) {
    unsigned int guard;

    SPSTAT = SPIF | WCOL;
    SPDAT = d;

    guard = 0;
    while (!(SPSTAT & SPIF))
    {
        guard++;
        if (guard > SPI_TIMEOUT)
        {
            return 0xFF;
        }
    }
    SPSTAT = SPIF | WCOL;

    return SPDAT;
}

void SPI_CLOCKS(unsigned char n) {
    unsigned char i;

    for (i = 0; i < n; i++)
    {
        SPI_XFER(0xFF);
    }
}

/*==================================================================
  粗测 SPI 时钟：连发 2000 字节 = 16000 个 bit
  时钟(kHz) = 16000 / 耗时(ms)
==================================================================*/
unsigned int SPI_CLOCK_MS(void) {
    unsigned int t;
    unsigned int i;

    TF_CS = 1;
    t = g_ms;
    for (i = 0; i < 2000; i++)
    {
        SPI_XFER(0xFF);
    }
    return (unsigned int)(g_ms - t);
}

/*==================================================================
  BMP581（气压计，4 线 SPI）
  ------------------------------------------------------------------
  ★手册 5.5.3：SPI 读【不需要 dummy 字节】
  ★寄存器：0x01 = CHIP_ID(0x50)，0x02 = REV_ID(0x32)
  CMD 字节 = bit7=R/W(读=1) | bit6:0=地址
==================================================================*/
/* 写一个寄存器：CMD 字节 bit7=0 表示写，后面跟\xCA\375据 */
/* prep SPI bus for BMP581: mode0/SPR0, MISO high-Z input, deselect others */
static void bmp_bus_prep(void) {
    SPI_SET_MODE(SPI_MODE0, SPI_SPR1);
    P1M1 |= 0x10;
    P1M0 &= ~0x10;
    TF_CS = 1;
    SPI_OLED_CS = 1;
    SPI_ROM_CS = 1;
}
void BMP_WRITE_REG(unsigned char reg, unsigned char val) {
    bmp_bus_prep();
    BMP_CS = 0;
    SPI_XFER((unsigned char)(reg & 0x7F));
    SPI_XFER(val);
    BMP_CS = 1;
}
void BMP_READ_BURST(unsigned char reg, unsigned char *buf, unsigned char n) {
    unsigned char i;

    bmp_bus_prep();
    BMP_CS = 0;
    SPI_XFER((unsigned char)(0x80 | (reg & 0x7F)));
    for (i = 0; i < n; i++)
    {
        buf[i] = SPI_XFER(0x00);
    }
    BMP_CS = 1;
}

unsigned char BMP_READ_REG(unsigned char reg) {
    unsigned char v;

    BMP_READ_BURST(reg, &v, 1);
    return v;
}

/*==================================================================
  TF 卡检测脚
==================================================================*/
/*==================================================================
  【临时】TF 卡\xC7\375动这一段暂时不参与编译
  ------------------------------------------------------------------
  原因：8051 的链接器不会给"没被调用的函\xCA\375"做 DATA 覆盖复用，
        每个函\xCA\375的局部变量各占一份，TF \xC7\375动单独就占了 40 多字节
        DATA，导致整个工程链接时 data 溢出（>128）。
  什么时候放回来：做"\xCA\375据记录到 TF 卡"功能时，把本段的 #if 0 改成 #if 1，
        并顺手把 TF_INIT / TF_WRITE_SECTOR 里的局部变量改成 static xdata。
==================================================================*/
#if 1
unsigned char TF_CD_LEVEL(void) {
    return TF_CD ? 1 : 0;
}

/*==================================================================
  原始 CMD0（诊断用）
  ------------------------------------------------------------------
  CS 拉高先给 80 个时钟，让卡进入就绪状态，再发 CMD0 并原样
  读回 n 个字节。SD 手册：卡应在 1~8 字节内给出 R1。
==================================================================*/
void TF_CMD0_RAW(unsigned char *buf, unsigned char n) {
    unsigned char i;

    TF_CS = 1;
    SPI_CLOCKS(10); /* 80 个时钟 */

    TF_CS = 0;
    SPI_XFER(0x40);
    SPI_XFER(0x00);
    SPI_XFER(0x00);
    SPI_XFER(0x00);
    SPI_XFER(0x00);
    SPI_XFER(0x95);

    for (i = 0; i < n; i++)
    {
        buf[i] = SPI_XFER(0xFF);
    }

    TF_CS = 1;
    SPI_XFER(0xFF);
}

/*==================================================================
  内部：发命令帧 + 等 R1，不碰 CS（CS 由调用者控制）
  ------------------------------------------------------------------
  命令帧 = 0x40|cmd, arg(4 字节 大端), CRC
  CMD0 的 CRC 固定 0x95；CMD8 固定 0x87；进 SPI 模式后 CRC 关掉，
  其它命令 CRC 随便填（0x01）
==================================================================*/
static unsigned char sd_send_cmd(unsigned char cmd, unsigned long arg, unsigned char crc,
                                 unsigned char ncr, unsigned char *r1) {
    unsigned char i;
    unsigned char b;

    TF_CS = 0;

    SPI_XFER((unsigned char)(0x40 | (cmd & 0x3F)));
    SPI_XFER((unsigned char)(arg >> 24));
    SPI_XFER((unsigned char)(arg >> 16));
    SPI_XFER((unsigned char)(arg >> 8));
    SPI_XFER((unsigned char)(arg));
    SPI_XFER(crc);

    b = 0xFF;
    for (i = 0; i < ncr; i++)
    {
        b = SPI_XFER(0xFF);
        if (!(b & 0x80))
        {
            break; /* bit7 = 0 -> 收到 R1 */
        }
    }
    *r1 = b;
    return (b & 0x80) ? 0 : 1;
}

/* 发命令 + 等 R1 + 释放 CS；返回 R1（0xFF = 没应答） */
static unsigned char sd_cmd_ok(unsigned char cmd, unsigned long arg, unsigned char crc) {
    unsigned char r1;
    unsigned char ok;

    ok = sd_send_cmd(cmd, arg, crc, 8, &r1);
    TF_CS = 1;
    SPI_XFER(0xFF);

    if (!ok)
    {
        return 0xFF;
    }
    return r1;
}

unsigned char TF_CMD_R(unsigned char cmd, unsigned long arg, unsigned char crc, unsigned char ncr,
                       unsigned char *buf, unsigned char extra) {
    unsigned char i;
    unsigned char ok;

    ok = sd_send_cmd(cmd, arg, crc, ncr, &buf[0]);

    if (!ok)
    {
        TF_CS = 1;
        SPI_XFER(0xFF);
        return 0;
    }

    for (i = 0; i < extra; i++)
    {
        buf[1 + i] = SPI_XFER(0xFF);
    }

    TF_CS = 1;
    SPI_XFER(0xFF);
    return 1;
}

/*==================================================================
  等\xCA\375据起始令牌 0xFE（CS 必须已经是低）
==================================================================*/
static unsigned char sd_wait_token(unsigned int limit) {
    unsigned int t;
    unsigned char b;

    for (t = 0; t < limit; t++)
    {
        b = SPI_XFER(0xFF);
        if (b == 0xFE)
        {
            return 1;
        }
        if (b != 0xFF)
        {
            return 0; /* 出错令牌 */
        }
    }
    return 0;
}

unsigned char TF_READ_REG16(unsigned char cmd, unsigned char *buf) {
    unsigned char r1;
    unsigned char i;

    if (!sd_send_cmd(cmd, 0x00000000UL, 0x01, 8, &r1) || r1 != 0x00)
    {
        TF_CS = 1;
        SPI_XFER(0xFF);
        return 0;
    }

    if (!sd_wait_token(60000U))
    {
        TF_CS = 1;
        SPI_XFER(0xFF);
        return 0;
    }

    for (i = 0; i < 16; i++)
    {
        buf[i] = SPI_XFER(0xFF);
    }
    SPI_XFER(0xFF); /* 丢 CRC */
    SPI_XFER(0xFF);

    TF_CS = 1;
    SPI_XFER(0xFF);
    return 1;
}

/*==================================================================
  lba -> 命令参\xCA\375
  SDSC 用字节地址，SDHC/SDXC 用块地址
==================================================================*/
static unsigned long sd_addr(unsigned long lba) {
    if (g_tf_sdhc)
    {
        return lba;
    }
    return lba * 512UL;
}

/*==================================================================
  初始化
  ------------------------------------------------------------------
  流程：\xC2\375速 CMD0 -> CMD8 -> ACMD41 -> CMD58(OCR) -> CMD9(CSD)
        -> CMD16(块长 512) -> 切快速
==================================================================*/
unsigned char TF_INIT(void) {
    unsigned char i;
    unsigned char r1;
    unsigned int c_size;
    unsigned char c_size_mult;
    unsigned char bl_len;
    unsigned long sectors;

    /* 1. 卡检测（插卡 = CD 低） */
    if (TF_CD_LEVEL() != 0)
    {
        return 1;
    }

    /* 2. \xC2\375速进 SPI 模式 */
    SPI_SET_MODE(SPI_MODE0, TF_SPR_INIT);
    TF_CS = 1;
    SPI_CLOCKS(10); /* 80 个时钟 */

    /* 3. CMD0 -> 必须回 0x01 */
    r1 = sd_cmd_ok(0x00, 0x00000000UL, 0x95);
    if (r1 != 0x01)
    {
        return 2;
    }

    /* 4. CMD8（老卡会回 0x05 = illegal command，忽略即可） */
    sd_cmd_ok(0x08, 0x000001AAUL, 0x87);

    /* 5. CMD55 + ACMD41(HCS=1) 等就绪 */
    r1 = 0xFF;
    for (i = 0; i < 100; i++)
    {
        sd_cmd_ok(0x37, 0x00000000UL, 0x01);      /* CMD55 = APP_CMD */
        r1 = sd_cmd_ok(0x29, 0x40000000UL, 0x01); /* ACMD41，HCS=1 */
        if (r1 == 0x00)
        {
            break;
        }
        delay_ms(10);
    }
    if (r1 != 0x00)
    {
        return 4;
    }

    /* 6. CMD58 读 OCR -> 判断 SDSC / SDHC */
    if (!TF_CMD_R(0x3A, 0x00000000UL, 0x01, 8, sd_rb, 4))
    {
        return 5;
    }
    g_tf_sdhc = (sd_rb[1] & 0x40) ? 1 : 0;

    /* 7. CMD9 读 CSD -> 算容量 */
    if (!TF_READ_REG16(0x09, sd_rb))
    {
        return 5;
    }
    if ((sd_rb[0] >> 6) == 0)
    {
        /* CSD v1.0（SDSC） */
        bl_len = (unsigned char)(sd_rb[5] & 0x0F);
        c_size =
            (unsigned int)(((unsigned int)(sd_rb[6] & 0x03) << 10) | ((unsigned int)sd_rb[7] << 2) |
                           ((unsigned int)(sd_rb[8] >> 6) & 0x03));
        c_size_mult = (unsigned char)(((sd_rb[9] & 0x03) << 1) | (sd_rb[10] >> 7));
        sectors = (unsigned long)(c_size + 1);
        i = (unsigned char)(c_size_mult + 2);
        if (bl_len >= 9)
        {
            i = (unsigned char)(i + (bl_len - 9));
            sectors <<= i;
        } else
        {
            sectors >>= (unsigned char)(9 - bl_len);
            sectors <<= i;
        }
    } else
    {
        /* CSD v2.0（SDHC/SDXC），单位固定 512 字节 */
        sectors = (((unsigned long)(sd_rb[7] & 0x3F) << 16) | ((unsigned long)sd_rb[8] << 8) |
                   (unsigned long)sd_rb[9]) +
                  1UL;
        sectors *= 1024UL;
    }
    g_tf_sectors = sectors;

    /* 8. CMD16 设块长 512（SDSC 读写前必须） */
    sd_cmd_ok(0x10, 0x00000200UL, 0x01);

    /* 9. 切到快速做\xCA\375据传输 */
    SPI_SET_MODE(SPI_MODE0, TF_SPR_XFER);

    return 0;
}

/*==================================================================
  读一个扇区（CMD17）
  返回 0 = 成功，1 = 命令失败，2 = 没等到\xCA\375据令牌
==================================================================*/
unsigned char TF_READ_SECTOR(unsigned long lba, unsigned char xdata *buf) {
    unsigned char r1;
    unsigned int i;

    SPI_SET_MODE(SPI_MODE0, TF_SPR_XFER);
    if (!sd_send_cmd(0x11, sd_addr(lba), 0x01, 8, &r1) || r1 != 0x00)
    {
        TF_CS = 1;
        SPI_XFER(0xFF);
        return 1;
    }

    if (!sd_wait_token(60000U))
    {
        TF_CS = 1;
        SPI_XFER(0xFF);
        return 2;
    }

    for (i = 0; i < 512; i++)
    {
        buf[i] = SPI_XFER(0xFF);
    }
    SPI_XFER(0xFF); /* 丢 CRC */
    SPI_XFER(0xFF);

    TF_CS = 1;
    SPI_XFER(0xFF);
    return 0;
}

/*==================================================================
  写一个扇区（CMD24）
  返回 0 = 成功，1 = 命令失败，2 = 卡拒收，3 = 写超时
==================================================================*/
unsigned char TF_WRITE_SECTOR(unsigned long lba, unsigned char xdata *buf) {
    unsigned char r1;
    unsigned char resp;
    unsigned int i;
    unsigned long t;

    if (!sd_send_cmd(0x18, sd_addr(lba), 0x01, 8, &r1) || r1 != 0x00)
    {
        TF_CS = 1;
        SPI_XFER(0xFF);
        return 1;
    }

    SPI_XFER(0xFF); /* 至少 1 字节间隔 */
    SPI_XFER(0xFE); /* \xCA\375据起始令牌 */

    for (i = 0; i < 512; i++)
    {
        SPI_XFER(buf[i]);
    }
    SPI_XFER(0xFF); /* 2 字节 CRC，SPI 模式不校验，随便填 */
    SPI_XFER(0xFF);

    /* \xCA\375据响应：低 5 位 = 0x05 表示已接收 */
    resp = SPI_XFER(0xFF);
    if ((resp & 0x1F) != 0x05)
    {
        TF_CS = 1;
        SPI_XFER(0xFF);
        return 2;
    }

    /* 等卡内部写完（MISO 一直被拉低，写完变高） */
    t = 0;
    while (SPI_XFER(0xFF) == 0x00)
    {
        t++;
        if (t > 200000UL)
        {
            TF_CS = 1;
            SPI_XFER(0xFF);
            return 3;
        }
    }

    TF_CS = 1;
    SPI_XFER(0xFF);
    return 0;
}

#endif /* TF \xC7\375动临时关闭 */
