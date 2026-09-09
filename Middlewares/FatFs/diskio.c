#include "diskio.h"

#include <stddef.h>

#include "./BSP/SDMMC/sdmmc_sdcard.h"

#define SD_DRIVE 0U

static DSTATUS g_sd_status = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != SD_DRIVE)
    {
        return STA_NOINIT;
    }

    return g_sd_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != SD_DRIVE)
    {
        return STA_NOINIT;
    }

    if (SD_Init() == SD_OK)
    {
        g_sd_status = 0U;
    }
    else
    {
        g_sd_status = STA_NOINIT;
    }

    return g_sd_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != SD_DRIVE || buff == NULL || count == 0U)
    {
        return RES_PARERR;
    }
    if ((g_sd_status & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    return (SD_ReadDisk(buff, sector, count) == 0U) ? RES_OK : RES_ERROR;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != SD_DRIVE || buff == NULL || count == 0U)
    {
        return RES_PARERR;
    }
    if ((g_sd_status & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    return (SD_WriteDisk((u8 *)buff, sector, count) == 0U) ? RES_OK : RES_ERROR;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != SD_DRIVE)
    {
        return RES_PARERR;
    }
    if ((g_sd_status & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            if (buff == NULL)
            {
                return RES_PARERR;
            }
            *(DWORD *)buff = (DWORD)(SDCardInfo.CardCapacity / 512LL);
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (buff == NULL)
            {
                return RES_PARERR;
            }
            *(WORD *)buff = 512U;
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (buff == NULL)
            {
                return RES_PARERR;
            }
            *(DWORD *)buff = 1U;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
