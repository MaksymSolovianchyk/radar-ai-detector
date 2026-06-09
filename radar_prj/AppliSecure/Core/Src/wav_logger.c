/*
 * wav_logger.c
 *
 *  Created on: Jun 1, 2026
 *      Author: Maksym
 */

/**
 * wav_logger.c
 * ------------
 * WAV file logger using FatFS.
 * Uses hsd2 from sd_card.c via SD_Card_IsPresent().
 *
 * WAV layout:
 *   Bytes  0-3   : "RIFF"
 *   Bytes  4-7   : file size - 8  (patched on close)
 *   Bytes  8-11  : "WAVE"
 *   Bytes 12-15  : "fmt "
 *   Bytes 16-19  : fmt chunk size = 16
 *   Bytes 20-21  : PCM = 1
 *   Bytes 22-23  : channels = 2
 *   Bytes 24-27  : sample rate = 3904
 *   Bytes 28-31  : byte rate = 3904 * 2 * 3 = 23424
 *   Bytes 32-33  : block align = 2 * 3 = 6
 *   Bytes 34-35  : bits per sample = 24
 *   Bytes 36-39  : "data"
 *   Bytes 40-43  : data size (patched on close)
 *   Bytes 44+    : PCM samples: CH0[3] CH1[3] CH0[3] CH1[3] ...
 */

#include "wav_logger.h"
#include "sd_card.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>

/* ── Constants ────────────────────────────────────────────────────────────── */
#define WAV_HEADER_SIZE    44U
#define BYTES_PER_SAMPLE   (WAV_NUM_CHANNELS * (WAV_BITS_PER_SAMPLE / 8U))  /* 6 */

/* ── Write buffer — batches writes to reduce SD access frequency ─────────── */
#define WRITE_BUF_SAMPLES  128U
#define WRITE_BUF_SIZE     (WRITE_BUF_SAMPLES * BYTES_PER_SAMPLE)   /* 768 B */

static uint8_t  write_buf[WRITE_BUF_SIZE] __attribute__((aligned(32)));
static uint32_t write_buf_pos = 0;

/* ── Private state ────────────────────────────────────────────────────────── */
static FATFS    fs;
static FIL      fil;
static char     filename[16];
static bool     ready        = false;
static uint32_t sample_count = 0;
static uint32_t flush_count  = 0;

/* ── Private helpers ──────────────────────────────────────────────────────── */

static void u16_le(uint8_t *buf, uint16_t v)
{
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)(v >> 8);
}

static void u32_le(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)(v        & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    buf[2] = (uint8_t)((v >>16) & 0xFF);
    buf[3] = (uint8_t)((v >>24) & 0xFF);
}

static void build_header(uint8_t *hdr, uint32_t data_size)
{
    const uint32_t byte_rate   = WAV_SAMPLE_RATE * WAV_NUM_CHANNELS *
                                 (WAV_BITS_PER_SAMPLE / 8U);
    const uint16_t block_align = (uint16_t)(WAV_NUM_CHANNELS *
                                 (WAV_BITS_PER_SAMPLE / 8U));

    memcpy(&hdr[0],  "RIFF", 4);
    u32_le(&hdr[4],  data_size + WAV_HEADER_SIZE - 8U);
    memcpy(&hdr[8],  "WAVE", 4);
    memcpy(&hdr[12], "fmt ", 4);
    u32_le(&hdr[16], 16);
    u16_le(&hdr[20], 1);                              /* PCM             */
    u16_le(&hdr[22], (uint16_t)WAV_NUM_CHANNELS);
    u32_le(&hdr[24], WAV_SAMPLE_RATE);
    u32_le(&hdr[28], byte_rate);
    u16_le(&hdr[32], block_align);
    u16_le(&hdr[34], (uint16_t)WAV_BITS_PER_SAMPLE);
    memcpy(&hdr[36], "data", 4);
    u32_le(&hdr[40], data_size);
}

static WAV_Result flush_buf(void)
{
    if (write_buf_pos == 0) return WAV_OK;
    UINT bw;
    FRESULT r = f_write(&fil, write_buf, write_buf_pos, &bw);
    if (r != FR_OK || bw != write_buf_pos) return WAV_ERR_WRITE;
    write_buf_pos = 0;
    return WAV_OK;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

WAV_Result WAV_Logger_Init(void)
{
    ready         = false;
    sample_count  = 0;
    flush_count   = 0;
    write_buf_pos = 0;

    /* Check SD card is present */
    if (!SD_Card_IsPresent())
        return WAV_ERR_NO_SD;

    /* Mount FatFS */
    if (f_mount(&fs, "", 1) != FR_OK)
        return WAV_ERR_MOUNT;

    /* Find free filename */
    FILINFO fno;
    uint16_t idx;
    for (idx = 0; idx <= 9999; idx++)
    {
        snprintf(filename, sizeof(filename), "REC_%04u.WAV", idx);
        if (f_stat(filename, &fno) == FR_NO_FILE) break;
    }
    if (idx > 9999) return WAV_ERR_OPEN;

    /* Create file */
    if (f_open(&fil, filename, FA_CREATE_NEW | FA_WRITE) != FR_OK)
        return WAV_ERR_OPEN;

    /* Write placeholder header */
    uint8_t hdr[WAV_HEADER_SIZE];
    build_header(hdr, 0);
    UINT bw;
    if (f_write(&fil, hdr, WAV_HEADER_SIZE, &bw) != FR_OK ||
        bw != WAV_HEADER_SIZE)
    {
        f_close(&fil);
        return WAV_ERR_WRITE;
    }
    f_sync(&fil);

    ready = true;
    return WAV_OK;
}


WAV_Result WAV_Logger_Write(const uint8_t *ch0_bytes,
                             const uint8_t *ch1_bytes)
{
    if (!ready) return WAV_ERR_NOT_INIT;

    /* Sign-extend 24-bit to 32-bit by shifting left 8 bits */
    int32_t ch0_32 = ((int32_t)ch0_bytes[0] << 24)
                   | ((int32_t)ch0_bytes[1] << 16)
                   | ((int32_t)ch0_bytes[2] <<  8);
    int32_t ch1_32 = ((int32_t)ch1_bytes[0] << 24)
                   | ((int32_t)ch1_bytes[1] << 16)
                   | ((int32_t)ch1_bytes[2] <<  8);

    /* Store as little-endian 32-bit */
    write_buf[write_buf_pos++] = (uint8_t)(ch0_32        & 0xFF);
    write_buf[write_buf_pos++] = (uint8_t)((ch0_32 >> 8) & 0xFF);
    write_buf[write_buf_pos++] = (uint8_t)((ch0_32 >>16) & 0xFF);
    write_buf[write_buf_pos++] = (uint8_t)((ch0_32 >>24) & 0xFF);

    write_buf[write_buf_pos++] = (uint8_t)(ch1_32        & 0xFF);
    write_buf[write_buf_pos++] = (uint8_t)((ch1_32 >> 8) & 0xFF);
    write_buf[write_buf_pos++] = (uint8_t)((ch1_32 >>16) & 0xFF);
    write_buf[write_buf_pos++] = (uint8_t)((ch1_32 >>24) & 0xFF);

    sample_count++;
    flush_count++;

    if (write_buf_pos >= WRITE_BUF_SIZE ||
        flush_count   >= WAV_FLUSH_EVERY)
    {
        flush_count = 0;
        WAV_Result r = flush_buf();
        if (r != WAV_OK) return r;
        f_sync(&fil);
    }

    return WAV_OK;
}


void WAV_Logger_Close(void)
{
    if (!ready) return;

    flush_buf();   /* write any remaining buffered data */

    /* Patch WAV header with real data size */
    uint32_t data_size = sample_count * BYTES_PER_SAMPLE;
    f_lseek(&fil, 0);
    uint8_t hdr[WAV_HEADER_SIZE];
    build_header(hdr, data_size);
    UINT bw;
    f_write(&fil, hdr, WAV_HEADER_SIZE, &bw);

    f_sync(&fil);
    f_close(&fil);
    f_mount(NULL, "", 0);

    ready = false;
}


bool WAV_Logger_IsReady(void)      { return ready; }
uint32_t WAV_Logger_GetSampleCount(void) { return sample_count; }
const char *WAV_Logger_GetFilename(void) { return filename; }
