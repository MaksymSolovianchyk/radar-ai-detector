/**
 * wav_logger.h
 * ------------
 * Records 24-bit stereo radar data (CH0=I, CH1=Q) to a WAV file
 * on the SD card using FatFS.
 *
 * Depends on:
 *   - sd_card.h / sd_card.c  (provides hsd2)
 *   - FatFS (ff.h, diskio.c)
 *
 * WAV file format:
 *   - PCM 24-bit stereo
 *   - Sample rate: 3904 Hz
 *   - Interleaved: CH0[3 bytes] CH1[3 bytes] per sample
 *   - Files named: REC_0000.WAV, REC_0001.WAV, ...
 *
 * Usage:
 *   WAV_Logger_Init()           — mount SD, create file, write header
 *   WAV_Logger_Write(ch0, ch1)  — call for every ADC sample
 *   WAV_Logger_Close()          — MUST call before power-off
 */

#ifndef WAV_LOGGER_H_
#define WAV_LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ── Result codes ─────────────────────────────────────────────────────────── */
typedef enum {
    WAV_OK           =  0,
    WAV_ERR_NO_SD    = -1,   /* SD card not initialised                      */
    WAV_ERR_MOUNT    = -2,   /* FatFS mount failed                           */
    WAV_ERR_OPEN     = -3,   /* Could not create file                        */
    WAV_ERR_WRITE    = -4,   /* Write failed                                 */
    WAV_ERR_NOT_INIT = -5,   /* WAV_Logger_Init() not called                 */
} WAV_Result;

/* ── Configuration ────────────────────────────────────────────────────────── */
#define WAV_SAMPLE_RATE      3904U   /* Hz — ADS131M04                       */
#define WAV_NUM_CHANNELS     2U      /* CH0 (I) + CH1 (Q)                    */
#define WAV_BITS_PER_SAMPLE  32U     /* 24-bit ADC                           */
#define WAV_FLUSH_EVERY      256U    /* flush to SD every N samples          */

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * @brief  Mount SD card filesystem and create a new WAV file.
 *         Writes a placeholder header — patched on WAV_Logger_Close().
 * @return WAV_OK on success, negative error code on failure.
 */
WAV_Result WAV_Logger_Init(void);

/**
 * @brief  Write one stereo sample to the WAV file.
 * @param  ch0_bytes  3 bytes of CH0, MSB first (ADC_Buffer.rx[3..5])
 * @param  ch1_bytes  3 bytes of CH1, MSB first (ADC_Buffer.rx[6..8])
 * @return WAV_OK on success, negative error code on failure.
 */
WAV_Result WAV_Logger_Write(const uint8_t *ch0_bytes,
                             const uint8_t *ch1_bytes);

/**
 * @brief  Flush all data, patch WAV header with real file size, close file.
 *         MUST be called before power-off — without this the file is invalid.
 */
void WAV_Logger_Close(void);

/**
 * @brief  Returns true if logger is ready to accept samples.
 */
bool WAV_Logger_IsReady(void);

/**
 * @brief  Returns number of samples written so far.
 */
uint32_t WAV_Logger_GetSampleCount(void);

/**
 * @brief  Returns current WAV filename (e.g. "REC_0003.WAV").
 */
const char *WAV_Logger_GetFilename(void);

#ifdef __cplusplus
}
#endif

#endif /* WAV_LOGGER_H_ */
