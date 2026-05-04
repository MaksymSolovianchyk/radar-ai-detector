/**
 * sd_logger.c
 * -----------
 * SD card CSV logger using FatFS.
 * Creates real files visible in Windows Explorer after formatting
 * the SD card as FAT32.
 *
 * File structure on SD card:
 *   LOG_0000.CSV
 *   LOG_0001.CSV
 *   ...
 *
 * Each file starts fresh on every power cycle.
 * Files are openable directly in Excel or any text editor.
 */

#include "sd_logger.h"
#include "ff.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* ── private state ────────────────────────────────────────────────────────── */
static FATFS    fs;
static FIL      fil;
static char     filename[16];
static bool     ready        = false;
static uint32_t row_count    = 0;
static uint32_t flush_count  = 0;

/* ── CSV header ───────────────────────────────────────────────────────────── */
static const char CSV_HEADER[] =
    "timestamp_ms,"
    "approaching_energy,"
    "receding_energy,"
    "approach_recede_ratio,"
    "max_approach_peak,"
    "max_recede_peak,"
    "total_energy,"
    "peak_doppler,"
    "velocity,"
    "center_of_mass,"
    "spectral_width\r\n";

/* ── public API ───────────────────────────────────────────────────────────── */

SD_Logger_Result SD_Logger_Init(void)
{
    ready       = false;
    row_count   = 0;
    flush_count = 0;

    /* Mount FAT filesystem */
    if (f_mount(&fs, "", 1) != FR_OK)
        return SD_LOGGER_ERR_INIT;

    /* Find first free filename LOG_0000.CSV → LOG_9999.CSV */
    FILINFO fno;
    uint16_t idx = 0;
    for (idx = 0; idx <= 9999; idx++)
    {
        snprintf(filename, sizeof(filename), "LOG_%04u.CSV", idx);
        if (f_stat(filename, &fno) == FR_NO_FILE)
            break;
    }
    if (idx > 9999)
        return SD_LOGGER_ERR_INIT;

    /* Create and open the file */
    if (f_open(&fil, filename, FA_CREATE_NEW | FA_WRITE) != FR_OK)
        return SD_LOGGER_ERR_INIT;

    /* Write CSV header */
    UINT bw;
    if (f_write(&fil, CSV_HEADER, strlen(CSV_HEADER), &bw) != FR_OK)
    {
        f_close(&fil);
        return SD_LOGGER_ERR_WRITE;
    }
    f_sync(&fil);   /* flush header immediately */

    ready = true;
    return SD_LOGGER_OK;
}


SD_Logger_Result SD_Logger_Write(const RadarFeatures_t *features)
{
    if (!ready)
        return SD_LOGGER_ERR_NOT_INIT;

    /* Build CSV row — all values in float format */
    char row[160];
    int len = snprintf(row, sizeof(row),
        "%lu,"      /* timestamp_ms            */
        "%.4f,"     /* approaching_energy       */
        "%.4f,"     /* receding_energy          */
        "%.4f,"     /* approach_recede_ratio    */
        "%.4f,"     /* max_approach_peak        */
        "%.4f,"     /* max_recede_peak          */
        "%.4f,"     /* total_energy             */
        "%.4f,"     /* peak_doppler             */
        "%.4f,"     /* velocity                 */
        "%.4f,"     /* center_of_mass           */
        "%.4f\r\n", /* spectral_width           */
        (unsigned long)HAL_GetTick(),
        features->approaching_energy,
        features->receding_energy,
        features->approach_recede_ratio,
        features->max_approach_peak,
        features->max_recede_peak,
        features->total_energy,
        features->peak_doppler,
        features->velocity,
        features->center_of_mass,
        features->spectral_width);

    if (len <= 0 || (size_t)len >= sizeof(row))
        return SD_LOGGER_ERR_WRITE;

    /* Write row */
    UINT bw;
    FRESULT res = f_write(&fil, row, (UINT)len, &bw);
    if (res != FR_OK || bw != (UINT)len)
        return SD_LOGGER_ERR_WRITE;

    row_count++;

    /* Flush every N rows to protect against power loss */
    if (++flush_count >= SD_LOGGER_FLUSH_INTERVAL)
    {
        flush_count = 0;
        f_sync(&fil);
    }

    return SD_LOGGER_OK;
}


void SD_Logger_Flush(void)
{
    if (!ready) return;
    f_sync(&fil);
    f_close(&fil);
    f_mount(NULL, "", 0);
    ready = false;
}


bool SD_Logger_IsReady(void)
{
    return ready;
}


uint32_t SD_Logger_GetRowCount(void)
{
    return row_count;
}
