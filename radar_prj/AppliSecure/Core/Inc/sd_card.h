/**
 * sd_card.h
 * ---------
 * SD card hardware initialisation and status.
 * Owns the SD_HandleTypeDef hsd2 and SDMMC2 peripheral setup.
 *
 * Usage:
 *   SD_Card_Init()      — call once at startup (after HAL_Init)
 *   SD_Card_IsPresent() — check if card initialised successfully
 */

#ifndef SD_CARD_H_
#define SD_CARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "stm32n6xx_hal.h"

/* ── Exported handle — used by FatFS diskio.c and wav_logger.c ───────────── */
extern SD_HandleTypeDef hsd2;

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise SDMMC2 peripheral and SD card.
 *         Soft-fail — if no card is inserted the system keeps running.
 * @return true if SD card initialised successfully, false otherwise.
 */
bool SD_Card_Init(void);

/**
 * @brief  Returns true if SD card was successfully initialised.
 */
bool SD_Card_IsPresent(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_H_ */
