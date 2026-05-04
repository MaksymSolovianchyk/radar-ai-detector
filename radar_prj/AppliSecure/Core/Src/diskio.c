/*-----------------------------------------------------------------------*/
/* diskio.c — FatFS disk I/O for STM32N6 SDMMC2 via HAL_SD              */
/*-----------------------------------------------------------------------*/

#include "ff.h"                      /* Must be first — defines BYTE, LBA_t, UINT */
#include "diskio.h"
#include "stm32n6xx_hal.h"           /* HAL_OK, HAL_GetTick etc. */
#include "stm32n6xx_hal_sd.h"        /* SD_HandleTypeDef, HAL_SD_* */

extern SD_HandleTypeDef hsd2;

#define SD_TIMEOUT_MS  5000U

/*-----------------------------------------------------------------------*/
/* Wait until SD card is ready                                           */
/*-----------------------------------------------------------------------*/
static DRESULT wait_ready(void)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_SD_GetCardState(&hsd2) != HAL_SD_CARD_TRANSFER)
    {
        if (HAL_GetTick() - t0 > SD_TIMEOUT_MS)
            return RES_ERROR;
    }
    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* disk_status                                                           */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return 0;
}

/*-----------------------------------------------------------------------*/
/* disk_initialize — SD already init'd by MX_SDMMC2_SD_Init() in main() */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return 0;
}

/*-----------------------------------------------------------------------*/
/* disk_read                                                             */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0) return RES_PARERR;

    if (HAL_SD_ReadBlocks(&hsd2, buff, (uint32_t)sector,
                          count, SD_TIMEOUT_MS) != HAL_OK)
        return RES_ERROR;

    return wait_ready();
}

/*-----------------------------------------------------------------------*/
/* disk_write                                                            */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0) return RES_PARERR;

    if (HAL_SD_WriteBlocks(&hsd2, (uint8_t *)buff, (uint32_t)sector,
                           count, SD_TIMEOUT_MS) != HAL_OK)
        return RES_ERROR;

    return wait_ready();
}
#endif

/*-----------------------------------------------------------------------*/
/* disk_ioctl                                                            */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;

    HAL_SD_CardInfoTypeDef info;

    switch (cmd)
    {
        case CTRL_SYNC:
            return wait_ready();

        case GET_SECTOR_COUNT:
            if (HAL_SD_GetCardInfo(&hsd2, &info) != HAL_OK)
                return RES_ERROR;
            *(LBA_t *)buff = info.LogBlockNbr;
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD *)buff = 512;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
