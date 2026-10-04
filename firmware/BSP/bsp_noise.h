#ifndef _BSP_NOISE_H
#define _BSP_NOISE_H

#include "STC8H.H"

/*==================================================================
  板载噪声模块（RS485 Modbus-RTU，9600-8-N-1，默认地址 1）
  ------------------------------------------------------------------
  寄存器 0x0000 (40001) = 瞬时噪声值 ×0.1 dB（只读，量程 30~130 dB）
      例：0x02C9 = 713 => 71.3 dB
  改地址  ：广播帧 FF 06 07 D0 00 <新地址> <CRC>
  改波特率：广播帧 FF 06 07 D1 00 <0=2400/1=4800/2=9600> <CRC>
==================================================================*/

typedef struct {
    unsigned char ok;  /* 1 = 读到有效值 */
    unsigned int db10; /* 噪声 ×0.1（713 = 71.3 dB） */
} noise_info_t;

void bsp_noise_init(void);

/* 每 1 秒读一次，主循环每圈调 */
void bsp_noise_poll(void);

noise_info_t *bsp_noise_get(void);

#endif
