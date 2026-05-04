#ifndef RADAR_FEATURES_H_
#define RADAR_FEATURES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32n6xx_hal.h"

/* TODO: Set actual radar parameters when known.
 * velocity = peak_doppler_hz * lambda / 2
 * lambda   = c / fc  (e.g. 24 GHz -> lambda ~ 0.01249 m)
 * Set RADAR_LAMBDA_M to 0.0f to output velocity = 0 until calibrated. */
#define RADAR_LAMBDA_ENABLED  1          /* 0 = off, 1 = on   */
#define RADAR_LAMBDA_M_VAL    0.01240f

typedef struct {
    float approaching_energy;
    float receding_energy;
    float approach_recede_ratio;
    float max_approach_peak;
    float max_recede_peak;
    float total_energy;
    float peak_doppler;   /* Hz – signed: >0 approaching, <0 receding */
    float velocity;       /* m/s – 0 until RADAR_LAMBDA_M is set      */
    float center_of_mass; /* Hz  */
    float spectral_width; /* Hz  */
} RadarFeatures_t;

/**
 * @brief  Compute radar features from a fftshifted magnitude array.
 *
 * @param  doppler_mag  Pointer to array of FFT_N magnitude values,
 *                      fftshifted so that DC is at index (num_bins/2).
 * @param  num_bins     Total number of bins (FFT_N).
 * @param  bin_hz       Hz per bin  = Fs / FFT_N.
 * @param  out          Output feature struct.
 */
void RadarFeatures_Compute(const float *doppler_mag,
                           uint32_t     num_bins,
                           float        bin_hz,
                           RadarFeatures_t *out);

/**
 * @brief  Transmit one CSV line over UART (blocking, small buffer).
 *         Format:
 *         timestamp_ms,app_e,rec_e,ratio,max_app,max_rec,
 *         total_e,peak_dop,vel,com,width\r\n
 */
void RadarFeatures_Print(UART_HandleTypeDef   *huart,
                         const RadarFeatures_t *features);

#ifdef __cplusplus
}
#endif

#endif /* RADAR_FEATURES_H_ */
