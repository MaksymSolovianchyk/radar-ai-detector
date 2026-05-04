#include "fft.h"
#include "main.h"

#define ARM_MATH_CM7
#define __FPU_PRESENT 1
#include "arm_math.h"

#if   FFT_N == 256
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len256;
#define FFT_INSTANCE  arm_cfft_sR_f32_len256
#elif FFT_N == 512
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len512;
#define FFT_INSTANCE  arm_cfft_sR_f32_len512
#elif FFT_N == 1024
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len1024;
#define FFT_INSTANCE  arm_cfft_sR_f32_len1024
#elif FFT_N == 2048
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len2048;
#define FFT_INSTANCE  arm_cfft_sR_f32_len2048
#elif FFT_N == 4096
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len4096;
#define FFT_INSTANCE  arm_cfft_sR_f32_len4096
#else
#error "FFT_N must be 256, 512, 1024, 2048 or 4096"
#endif

#include <string.h>
#include <math.h>

extern UART_HandleTypeDef huart1;

#define FFT_N2           (FFT_N * 2u)
#define DC_GUARD_BINS    2u
#define FFT_STARTUP_GUARD 4u

/* Single input buffer — startup guard replaces ping-pong (saves 8 KB).
 * fft_mag eliminated — magnitude written directly into fft_tx_buf (saves 4 KB).
 * fft_window eliminated — Hamming computed on the fly (saves 4 KB).
 * Total saving vs previous version: 16 KB.                              */
static float   fft_input[FFT_N2];
static uint8_t fft_tx_buf[FFT_PACKET_BYTES];

/* Pointer to the float payload region inside fft_tx_buf.               */
#define TX_MAG_PTR  ((float *)(&fft_tx_buf[4]))

static volatile uint32_t sample_count = 0;
static volatile bool     frame_ready  = false;
static volatile bool     tx_done      = true;


static uint8_t checksum_xor(const uint8_t *buf, uint32_t len)
{
    uint8_t cs = 0;
    for (uint32_t i = 0; i < len; i++) cs ^= buf[i];
    return cs;
}

static void fftshift_inplace(float *buf, uint32_t n)
{
    uint32_t half = n / 2u;
    float tmp;
    for (uint32_t i = 0; i < half; i++)
    {
        tmp           = buf[i];
        buf[i]        = buf[i + half];
        buf[i + half] = tmp;
    }
}

static inline float hamming(uint32_t n)
{
    return 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)n / (float)(FFT_N - 1u));
}


void FFT_Init(void)
{
    memset(fft_input,  0, sizeof(fft_input));
    memset(fft_tx_buf, 0, sizeof(fft_tx_buf));
    sample_count = 0;
    frame_ready  = false;
    tx_done      = true;
}


void FFT_PushSample(float i, float q)
{
    if (frame_ready) return;

    if (sample_count < FFT_STARTUP_GUARD)
    {
        sample_count++;
        return;
    }

    uint32_t idx = sample_count - FFT_STARTUP_GUARD;
    if (idx < FFT_N)
    {
        fft_input[idx * 2u]      = i;
        fft_input[idx * 2u + 1u] = q;
        sample_count++;
        if ((sample_count - FFT_STARTUP_GUARD) >= FFT_N)
            frame_ready = true;
    }
}


bool FFT_IsReady(void)
{
    return frame_ready;
}


void FFT_Process(void)
{
    if (!frame_ready) return;

    float sum_i = 0.0f, sum_q = 0.0f;
    for (uint32_t n = 0; n < FFT_N; n++)
    {
        sum_i += fft_input[n * 2u];
        sum_q += fft_input[n * 2u + 1u];
    }
    float mean_i = sum_i / (float)FFT_N;
    float mean_q = sum_q / (float)FFT_N;

    for (uint32_t n = 0; n < FFT_N; n++)
    {
        float w = hamming(n);
        fft_input[n * 2u]      = (fft_input[n * 2u]      - mean_i) * w;
        fft_input[n * 2u + 1u] = (fft_input[n * 2u + 1u] - mean_q) * w;
    }

    arm_cfft_f32(&FFT_INSTANCE, fft_input, 0, 1);

    arm_cmplx_mag_f32(fft_input, TX_MAG_PTR, FFT_N);

    fftshift_inplace(TX_MAG_PTR, FFT_N);

    uint16_t sync  = (uint16_t)FFT_SYNC_WORD;
    uint16_t count = (uint16_t)FFT_N;
    fft_tx_buf[0] = (uint8_t)(sync  & 0xFFu);
    fft_tx_buf[1] = (uint8_t)(sync  >> 8u);
    fft_tx_buf[2] = (uint8_t)(count & 0xFFu);
    fft_tx_buf[3] = (uint8_t)(count >> 8u);
    fft_tx_buf[FFT_PACKET_BYTES - 1u] = checksum_xor(&fft_tx_buf[4], FFT_N * sizeof(float));

    sample_count = 0;
    frame_ready  = false;
}


void FFT_Transmit(void)
{
    tx_done = false;
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(&huart1, fft_tx_buf, (uint16_t)FFT_PACKET_BYTES);
    if (status != HAL_OK)tx_done = true;
}


bool FFT_TransmitDone(void)
{
    return tx_done;
}


void FFT_Reset(void)
{
    memset(fft_input, 0, sizeof(fft_input));
    sample_count = 0;
    frame_ready  = false;
}


void FFT_TxCompleteCallback(void)
{
    tx_done = true;
}


const float* FFT_GetMagnitude(void)
{
    return TX_MAG_PTR;
}


float FFT_GetPeakFrequency(void)
{
    float    peak_mag = 0.0f;
    uint32_t peak_idx = FFT_N / 2u;
    float    bin_hz   = (float)FFT_FS / (float)FFT_N;
    uint32_t dc       = FFT_N / 2u;
    const float *mag  = TX_MAG_PTR;

    for (uint32_t k = 0; k < FFT_N; k++)
    {
        if (k >= (dc - DC_GUARD_BINS) && k <= (dc + DC_GUARD_BINS)) continue;
        if (mag[k] > peak_mag)
        {
            peak_mag = mag[k];
            peak_idx = k;
        }
    }

    return ((float)peak_idx - (float)dc) * bin_hz;
}
