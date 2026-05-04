/**
 * sd_logger.h
 * -----------
 * SD card CSV logger using FatFS.
 * Creates real FAT32 files visible in Windows Explorer.
 *
 * Requirements:
 *   - SD card formatted as FAT32 (right-click in Windows → Format → FAT32)
 *   - FatFS source files (ff.c, ff.h, ffconf.h) in project
 *   - diskio.c glue layer connecting FatFS to hsd2
 *
 * Result on SD card after recording:
 *   LOG_0000.CSV  ← openable in Excel directly
 *   LOG_0001.CSV  ← new file each power cycle
 *   ...
 */

#ifndef SD_LOGGER_H_
#define SD_LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "radar_features.h"

/* ── result codes ─────────────────────────────────────────────────────────── */
typedef enum {
    SD_LOGGER_OK           =  0,
    SD_LOGGER_ERR_INIT     = -1,
    SD_LOGGER_ERR_WRITE    = -2,
    SD_LOGGER_ERR_NOT_INIT = -3,
} SD_Logger_Result;

/* ── configuration ────────────────────────────────────────────────────────── */
#define SD_LOGGER_FLUSH_INTERVAL  20U   /* flush to card every N rows */

/* ── public API ───────────────────────────────────────────────────────────── */
SD_Logger_Result SD_Logger_Init(void);
SD_Logger_Result SD_Logger_Write(const RadarFeatures_t *features);
void             SD_Logger_Flush(void);
bool             SD_Logger_IsReady(void);
uint32_t         SD_Logger_GetRowCount(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_LOGGER_H_ */
