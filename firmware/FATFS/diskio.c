#include "diskio.h"
#include "ff.h"
#include "spi.h"

/* FatFs 的底层磁盘 I/O，对接 spi.c 里的 TF/SD 扇区驱动 */

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0)
    {
        return STA_NOINIT;
    }
    if (TF_INIT() != 0)
    {
        return STA_NOINIT;
    }
    return 0;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0)
    {
        return STA_NOINIT;
    }
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    UINT i;

    if (pdrv != 0)
    {
        return RES_PARERR;
    }
    for (i = 0; i < count; i++)
    {
        if (TF_READ_SECTOR((unsigned long)(sector + i),
                           (unsigned char xdata *)buff + (unsigned int)i * 512U) != 0)
        {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    UINT i;

    if (pdrv != 0)
    {
        return RES_PARERR;
    }
    for (i = 0; i < count; i++)
    {
        if (TF_WRITE_SECTOR((unsigned long)(sector + i),
                            (unsigned char xdata *)(buff + (unsigned int)i * 512U)) != 0)
        {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0)
    {
        return RES_PARERR;
    }
    switch (cmd)
    {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = g_tf_sectors;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

/* 文件时间戳：本板无 RTC 走 FatFs 时钟，返回固定时间（2026-01-01 00:00:00） */
DWORD get_fattime(void) {
    return ((DWORD)(2026 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
