/*
 * radar_features.c
 *
 * Computes Doppler-domain radar features from the fftshifted magnitude
 * array produced by fft.c / FFT_Process().
 *
 * After FFT_Process() the magnitude array is fftshifted:
 *   index 0           -> most-negative Doppler (max receding)
 *   index num_bins/2  -> DC (0 Hz, static targets)
 *   index num_bins-1  -> most-positive Doppler (max approaching)
 *
 * We exclude a narrow DC guard band (DC_GUARD_BINS on each side of DC)
 * matching the guard already used in FFT_GetPeakFrequency().
 */

#include "radar_features.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Number of bins around DC to treat as static / suppress.
 * Must match DC_GUARD_BINS in fft.c (currently 2). */
#define RF_DC_GUARD  2u

/* Small value to avoid division by zero. */
#define RF_EPS       1e-6f

/* ------------------------------------------------------------------ */
void RadarFeatures_Compute(const float     *doppler_mag,
                           uint32_t         num_bins,
                           float            bin_hz,
                           RadarFeatures_t *out)
{
    if (!doppler_mag || !out || num_bins < 4u) return;

    const uint32_t dc = num_bins / 2u;   /* DC bin index after fftshift */

    /* Zero the output struct first. */
    memset(out, 0, sizeof(RadarFeatures_t));

    /* --------------------------------------------------------------- *
     * Single pass: accumulate energies, track peaks, weighted sums.   *
     * Receding  = bins [0 .. dc-guard-1]  (negative Doppler)          *
     * Approaching = bins [dc+guard+1 .. num_bins-1] (positive Doppler)*
     * --------------------------------------------------------------- */
    float weighted_freq_sum = 0.0f;
    float weight_sum        = 0.0f;
    float peak_mag          = 0.0f;
    uint32_t peak_bin       = dc + RF_DC_GUARD + 1u;  /* default: first approach bin */

    for (uint32_t k = 0u; k < num_bins; k++)
    {
        /* Skip DC guard band */
        if (k >= (dc - RF_DC_GUARD) && k <= (dc + RF_DC_GUARD))
            continue;

        float mag = doppler_mag[k];
        if (mag < 0.0f) mag = 0.0f;   /* safety clamp */

        /* Signed frequency of this bin (Hz) */
        float freq_hz = ((float)k - (float)dc) * bin_hz;

        out->total_energy += mag;

        if (k > dc)   /* approaching (positive Doppler) */
        {
            out->approaching_energy += mag;
            if (mag > out->max_approach_peak)
                out->max_approach_peak = mag;
        }
        else          /* receding (negative Doppler) */
        {
            out->receding_energy += mag;
            if (mag > out->max_recede_peak)
                out->max_recede_peak = mag;
        }

        /* Track global peak for peak_doppler */
        if (mag > peak_mag)
        {
            peak_mag = mag;
            peak_bin = k;
        }

        /* Accumulate for centre-of-mass */
        weighted_freq_sum += freq_hz * mag;
        weight_sum        += mag;
    }

    /* --------------------------------------------------------------- */
    /* Derived scalar features                                          */
    /* --------------------------------------------------------------- */
    out->approach_recede_ratio =
        out->approaching_energy / (out->receding_energy + RF_EPS);

    out->peak_doppler = ((float)peak_bin - (float)dc) * bin_hz;

    /* velocity = peak_doppler * lambda / 2  (radial velocity in m/s)  */
#if RADAR_LAMBDA_ENABLED
    out->velocity = out->peak_doppler * (RADAR_LAMBDA_M_VAL / 2.0f);
#else
    out->velocity = 0.0f;  /* TODO: set RADAR_LAMBDA_M in radar_features.h */
#endif

    if (weight_sum > RF_EPS)
    {
        out->center_of_mass = weighted_freq_sum / weight_sum;

        /* Spectral width = weighted standard deviation (Hz) */
        float var_sum = 0.0f;
        for (uint32_t k = 0u; k < num_bins; k++)
        {
            if (k >= (dc - RF_DC_GUARD) && k <= (dc + RF_DC_GUARD))
                continue;

            float mag     = doppler_mag[k];
            if (mag < 0.0f) mag = 0.0f;
            float freq_hz = ((float)k - (float)dc) * bin_hz;
            float diff    = freq_hz - out->center_of_mass;
            var_sum      += diff * diff * mag;
        }
        out->spectral_width = sqrtf(var_sum / weight_sum);
    }
    else
    {
        out->center_of_mass = 0.0f;
        out->spectral_width = 0.0f;
    }
}

/* ------------------------------------------------------------------ */
void RadarFeatures_Print(UART_HandleTypeDef    *huart,
                         const RadarFeatures_t  *features)
{
    if (!huart || !features) return;

    static uint32_t frame_count = 0u;
    uint32_t ts_ms = HAL_GetTick();

    /* Print CSV header once at startup */
    if (frame_count == 0u)
    {
        const char *hdr =
            "timestamp_ms,approaching_energy,receding_energy,"
            "approach_recede_ratio,max_approach_peak,max_recede_peak,"
            "total_energy,peak_doppler,velocity,center_of_mass,"
            "spectral_width\r\n";
        HAL_UART_Transmit(huart, (uint8_t *)hdr, (uint16_t)strlen(hdr),
                          HAL_MAX_DELAY);
    }
    frame_count++;

    /* Build data line – snprintf into a local buffer.
     * Max line ~200 chars; stack usage is acceptable in main context. */
    char buf[220];
    int len = snprintf(buf, sizeof(buf),
        "%lu,%.2f,%.2f,%.4f,%.2f,%.2f,%.2f,%.2f,%.4f,%.2f,%.2f\r\n",
        (unsigned long)ts_ms,
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

    if (len > 0 && (size_t)len < sizeof(buf))
        HAL_UART_Transmit(huart, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
}
