#ifndef __i2c_h__
#define __i2c_h__

#include "STC8H.H"

/* 软件 I2C（位翻转），引脚开漏输出，靠 R24/R25=4.7k 上拉
   SDA = P2.4    SCL = P2.5
   本板从机地址（硬件文档 4.1）：
     IST8310  0x0C   磁力计
     VEML7700 0x10   光照
     SHT45    0x44   温湿度
     PCF8563  0x51   RTC
     SCD41    0x62   CO2                                    */

void I2C_INIT(void);

/* 扫描 0x08 ~ 0x77，把应答的地址写进 buf，返回找到的个数 */
unsigned char I2C_SCAN(unsigned char *buf, unsigned char max);

/* 读寄存器：成功返回 1 */
unsigned char I2C_READ_REG(unsigned char addr7, unsigned char reg,
                           unsigned char *val);

/* 写寄存器：成功返回 1 */
unsigned char I2C_WRITE_REG(unsigned char addr7, unsigned char reg,
                            unsigned char val);

/* write 16-bit register (LSB then MSB), success = 1 */
unsigned char I2C_WRITE_REG16(unsigned char addr7, unsigned char reg, unsigned int val);

/* Only probe whether the address ACKs (no register access).
   Use this for devices with a 16-bit command protocol (SHT45 / SCD41). */
unsigned char I2C_PING(unsigned char addr7);
/* 读 16 位寄存器（低字节在前），成功返回 1 */
unsigned char I2C_READ_REG16(unsigned char addr7, unsigned char reg, unsigned int *val);

/* 发 16 位命令（高字节在前），然后读 n 个字节；成功返回 1 */
unsigned char I2C_CMD_READ(unsigned char addr7, unsigned int cmd, unsigned char *buf, unsigned char n);
/* 只发命令（不读） */
unsigned char I2C_SEND_CMD(unsigned char addr7, unsigned int cmd);

/* 直接读 n 个字节（用于"发命令 -> 等待 -> 再读"的器件） */
unsigned char I2C_READ_BYTES(unsigned char addr7, unsigned char *buf, unsigned char n);

#endif
