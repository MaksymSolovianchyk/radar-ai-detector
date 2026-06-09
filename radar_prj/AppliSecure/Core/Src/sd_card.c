/**
 * sd_card.c
 * ---------
 * SD card hardware initialisation.
 * Owns hsd2 and MX_SDMMC2_SD_Init logic.
 * Isolated here so main.c and wav_logger.c stay clean.
 *
 * STM32N6570-DK SD card pins (SDMMC2):
 *   PC0 -> D2    PC2 -> CK    PC3 -> CMD
 *   PC4 -> D0    PC5 -> D1    PE4 -> D3
 */

#include "sd_card.h"

/* ── SD handle definition ─────────────────────────────────────────────────── */
/* Declared extern in sd_card.h so diskio.c and wav_logger.c can use it       */
SD_HandleTypeDef hsd2;

/* ── Private state ────────────────────────────────────────────────────────── */
static bool sd_present = false;

/* ── Public API ───────────────────────────────────────────────────────────── */

bool SD_Card_Init(void)
{
    sd_present = false;

    hsd2.Instance                 = SDMMC2;
    hsd2.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd2.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd2.Init.BusWide             = SDMMC_BUS_WIDE_4B;
    hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd2.Init.ClockDiv            = 2;

    if (HAL_SD_Init(&hsd2) != HAL_OK)
    {
        /* Soft-fail — no card inserted or hardware issue */
        return false;
    }

    sd_present = true;
    return true;
}


bool SD_Card_IsPresent(void)
{
    return sd_present;
}
