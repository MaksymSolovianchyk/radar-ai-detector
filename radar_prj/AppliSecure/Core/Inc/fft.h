#ifndef FFT_H_
#define FFT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define FFT_N           256u//change for desired number of samples 1024u for fft_script.py
#define FFT_FS          3904u
#define FFT_SYNC_WORD   0xAA55u
#define FFT_PACKET_BYTES  (2u + 2u + (FFT_N * 4u) + 1u)

#define FFT_ADC_FSR   0.15f
#define FFT_ADC_STEP  (FFT_ADC_FSR / 8388608.0f)

static inline float adc_to_float(uint8_t msb, uint8_t mid, uint8_t lsb)
{
    int32_t raw = ((int32_t)msb << 16) | ((int32_t)mid << 8) | (int32_t)lsb;
    if (raw & 0x00800000) raw |= 0xFF000000;
    return (float)raw * FFT_ADC_STEP;
}

void         FFT_Init(void);
void         FFT_PushSample(float i, float q);
bool         FFT_IsReady(void);
void         FFT_Process(void);
void         FFT_Transmit(void);
bool         FFT_TransmitDone(void);
void         FFT_Reset(void);
void         FFT_TxCompleteCallback(void);
const float* FFT_GetMagnitude(void);
float        FFT_GetPeakFrequency(void);

#ifdef __cplusplus
}
#endif

#endif
