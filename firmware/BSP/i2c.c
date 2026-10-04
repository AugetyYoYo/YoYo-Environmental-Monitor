#include "i2c.h"
#include "Config.h"
#include "intrins.h"

sbit I2C_SDA = P2 ^ 4;
sbit I2C_SCL = P2 ^ 5;

/* 半时钟延时：约 10 次空循环 -> ~1.7us -> 约 290kHz */
static void i2c_dly(void) {
  unsigned char i;

  /* 30 次空循环 -> 约 100kHz，给 4.7k 上拉的上\xC9\375沿留足时间 */
  for (i = 0; i < 30; i++) {
    _nop_();
  }
}

static void scl_h(void) {
  I2C_SCL = 1;
  i2c_dly();
}
static void scl_l(void) {
  I2C_SCL = 0;
  i2c_dly();
}
static void sda_h(void) {
  I2C_SDA = 1;
  i2c_dly();
}
static void sda_l(void) {
  I2C_SDA = 0;
  i2c_dly();
}

void I2C_INIT(void) {
  /* P2.4 / P2.5 开漏输出（M1:M0 = 11），高电平靠外部上拉 */
  P2M1 |= 0x30;
  P2M0 |= 0x30;

  I2C_SDA = 1;
  I2C_SCL = 1;
}

static void i2c_start(void) {
  sda_h();
  scl_h();
  sda_l();
  scl_l();
}

static void i2c_stop(void) {
  sda_l();
  scl_h();
  sda_h();
  i2c_dly();
}

/* 写一个字节，返回 0 = 收到应答 */
static unsigned char i2c_write_byte(unsigned char d) {
  unsigned char i;
  unsigned char ack;

  for (i = 0; i < 8; i++) {
    if (d & 0x80) {
      sda_h();
    } else {
      sda_l();
    }
    scl_h();
    scl_l();
    d <<= 1;
  }

  sda_h(); /* 释放 SDA 给从机 */
  scl_h();
  ack = I2C_SDA;
  scl_l();

  return ack;
}

static unsigned char i2c_read_byte(unsigned char ack) {
  unsigned char i;
  unsigned char d;

  d = 0;
  sda_h();

  for (i = 0; i < 8; i++) {
    d <<= 1;
    scl_h();
    if (I2C_SDA) {
      d |= 0x01;
    }
    scl_l();
  }

  if (ack) {
    sda_h();
  } else {
    sda_l();
  }
  scl_h();
  scl_l();
  sda_h();

  return d;
}

static unsigned char i2c_probe(unsigned char addr7) {
  unsigned char ack;

  i2c_start();
  ack = i2c_write_byte((unsigned char)(addr7 << 1));
  i2c_stop();

  if (ack == 0) {
    return 1;
  }
  return 0;
}

unsigned char I2C_SCAN(unsigned char *buf, unsigned char max) {
  unsigned char addr;
  unsigned char n;

  n = 0;
  for (addr = 0x08; addr <= 0x77; addr++) {
    if (i2c_probe(addr)) {
      if (n < max) {
        buf[n] = addr;
        n++;
      }
    }
  }
  return n;
}

unsigned char I2C_READ_REG(unsigned char addr7, unsigned char reg,
                           unsigned char *val) {
  unsigned char ack;

  i2c_start();
  ack = i2c_write_byte((unsigned char)(addr7 << 1));
  if (ack) {
    i2c_stop();
    return 0;
  }
  ack = i2c_write_byte(reg);
  if (ack) {
    i2c_stop();
    return 0;
  }

  i2c_start();
  ack = i2c_write_byte((unsigned char)((addr7 << 1) | 0x01));
  if (ack) {
    i2c_stop();
    return 0;
  }
  *val = i2c_read_byte(1);
  i2c_stop();

  return 1;
}

unsigned char I2C_WRITE_REG(unsigned char addr7, unsigned char reg,
                            unsigned char val) {
  unsigned char ack;

  i2c_start();
  ack = i2c_write_byte((unsigned char)(addr7 << 1));
  if (ack) {
    i2c_stop();
    return 0;
  }
  ack = i2c_write_byte(reg);
  if (ack) {
    i2c_stop();
    return 0;
  }
  ack = i2c_write_byte(val);
  i2c_stop();

  if (ack == 0) {
    return 1;
  }
  return 0;
}

/* write 16-bit register: reg address, then LSB, then MSB (VEML7700 etc.) */
unsigned char I2C_WRITE_REG16(unsigned char addr7, unsigned char reg, unsigned int val) {
  unsigned char ack;

  i2c_start();
  ack = i2c_write_byte((unsigned char)(addr7 << 1));
  if (ack) { i2c_stop(); return 0; }
  ack = i2c_write_byte(reg);
  if (ack) { i2c_stop(); return 0; }
  ack = i2c_write_byte((unsigned char)val);
  if (ack) { i2c_stop(); return 0; }
  ack = i2c_write_byte((unsigned char)(val >> 8));
  i2c_stop();
  return (unsigned char)(ack == 0);
}
/* ACK-only probe */
unsigned char I2C_PING(unsigned char addr7)
{
  return i2c_probe(addr7);
}

unsigned char I2C_READ_REG16(unsigned char addr7, unsigned char reg, unsigned int *val) {
  unsigned char lo;
  unsigned char hi;

  if (!I2C_READ_REG(addr7, reg, &lo)) {
    return 0;
  }
  if (!I2C_READ_REG(addr7, (unsigned char)(reg + 1), &hi)) {
    return 0;
  }
  *val = ((unsigned int)hi << 8) | lo;
  return 1;
}

unsigned char I2C_CMD_READ(unsigned char addr7, unsigned int cmd, unsigned char *buf, unsigned char n) {
  unsigned char ack;
  unsigned char i;

  i2c_start();
  ack = i2c_write_byte((unsigned char)(addr7 << 1));
  if (ack) {
    i2c_stop();
    return 0;
  }
  if (cmd > 0xFF) {
    ack = i2c_write_byte((unsigned char)(cmd >> 8));
    if (ack) {
      i2c_stop();
      return 0;
    }
  }
  ack = i2c_write_byte((unsigned char)cmd);
  if (ack) {
    i2c_stop();
    return 0;
  }

  i2c_start();
  ack = i2c_write_byte((unsigned char)((addr7 << 1) | 0x01));
  if (ack) {
    i2c_stop();
    return 0;
  }
  for (i = 0; i < n; i++) {
    buf[i] = i2c_read_byte((unsigned char)(i == (unsigned char)(n - 1)));
  }
  i2c_stop();
  return 1;
}

unsigned char I2C_SEND_CMD(unsigned char addr7, unsigned int cmd) {
  unsigned char ack;

  i2c_start();
  ack = i2c_write_byte((unsigned char)(addr7 << 1));
  if (ack) {
    i2c_stop();
    return 0;
  }
  if (cmd > 0xFF) {
    ack = i2c_write_byte((unsigned char)(cmd >> 8));
    if (ack) {
      i2c_stop();
      return 0;
    }
  }
  ack = i2c_write_byte((unsigned char)cmd);
  i2c_stop();
  if (ack == 0) {
    return 1;
  }
  return 0;
}

unsigned char I2C_READ_BYTES(unsigned char addr7, unsigned char *buf, unsigned char n) {
  unsigned char ack;
  unsigned char i;

  i2c_start();
  ack = i2c_write_byte((unsigned char)((addr7 << 1) | 0x01));
  if (ack) {
    i2c_stop();
    return 0;
  }
  for (i = 0; i < n; i++) {
    buf[i] = i2c_read_byte((unsigned char)(i == (unsigned char)(n - 1)));
  }
  i2c_stop();
  return 1;
}