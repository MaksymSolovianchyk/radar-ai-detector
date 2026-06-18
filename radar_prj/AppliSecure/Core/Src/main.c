/* USER CODE BEGIN Header */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "radar.h"
#include "ads131m0x.h"
#include "fft.h"
<<<<<<< HEAD
#include "radar_features.h"
=======
>>>>>>> 4d2ac1a6586fd99d736593ef7b227981f5e2f31d
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define VECT_TAB_NS_OFFSET       0x00400
#define VTOR_TABLE_NS_START_ADDR (SRAM2_AXI_BASE_NS | VECT_TAB_NS_OFFSET)
#define ADC_RESET_Pin            GPIO_PIN_3
#define ADC_RESET_GPIO_Port      GPIOB
#define ADC_FSR                  0.15f
#define ADC_STEP                 (ADC_FSR / 8388608.0f)
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef  hspi5;
DMA_HandleTypeDef  handle_GPDMA1_Channel0;
DMA_HandleTypeDef  handle_GPDMA1_Channel1;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  handle_GPDMA1_Channel2;
DMA_HandleTypeDef  handle_GPDMA1_Channel3;

/* USER CODE BEGIN PV */
extern volatile uint8_t  cmd_buffer[3];
extern volatile int      commandflag;
extern volatile bool     adc_data_ready;
extern volatile bool     adcready;

volatile uint32_t dma_transfer_count = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void SystemIsolation_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_SPI5_Init(void);
static void MX_USART1_UART_Init(void);

/* ═══════════════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();

    MX_GPIO_Init();
    MX_GPDMA1_Init();
    MX_SPI5_Init();
    MX_USART1_UART_Init();
    SystemIsolation_Config();

    /* USER CODE BEGIN 2 */

    /* ── Step 1: initialise ADC first so it releases MISO ───────────────── */
    ADS_Init();
    UartStartReceive();
<<<<<<< HEAD

=======
    FFT_Init();
>>>>>>> 4d2ac1a6586fd99d736593ef7b227981f5e2f31d
    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */
    while (1)
        {
<<<<<<< HEAD

                if (FFT_IsReady())
                {
                    FFT_Process();

                    RadarFeatures_t rf;
                    RadarFeatures_Compute(FFT_GetMagnitude(),
                                          FFT_N,
                                          (float)FFT_FS / (float)FFT_N,
                                          &rf);

                    /* CSV line — matches parse_line() in live_infer.py exactly:
                     * timestamp_ms,f0..f9                                        */
                    RadarFeatures_Print(&huart1, &rf);

                    while (!FFT_TransmitDone()) { __NOP(); }
                    FFT_Reset();
                }


            if (commandflag)
            {
                __disable_irq();

=======
            if (adc_data_ready)
            {
                adc_data_ready = false;

                int32_t raw_ch0 = ((int32_t)ADC_Buffer.rx[3] << 16)
                                | ((int32_t)ADC_Buffer.rx[4] << 8)
                                | ((int32_t)ADC_Buffer.rx[5]);

                if (raw_ch0 & 0x800000)
                    raw_ch0 -= 0x1000000;

                int32_t raw_ch1 = ((int32_t)ADC_Buffer.rx[6] << 16)
                                | ((int32_t)ADC_Buffer.rx[7] << 8)
                                | ((int32_t)ADC_Buffer.rx[8]);

                if (raw_ch1 & 0x800000)
                    raw_ch1 -= 0x1000000;

                float ch0_v = (float)raw_ch0 * ADC_STEP;
                float ch1_v = (float)raw_ch1 * ADC_STEP;

                char msg[80];
                int len = snprintf(msg, sizeof(msg),
                                   "CH0: %8ld %+.6fV | CH1: %8ld %+.6fV\r\n",
                                   raw_ch0,
                                   ch0_v,
                                   raw_ch1,
                                   ch1_v);

                HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, 100);
            }

            if (commandflag)
            {
                __disable_irq();

>>>>>>> 4d2ac1a6586fd99d736593ef7b227981f5e2f31d
                uint8_t local_cmd[3];
                local_cmd[0] = cmd_buffer[0];
                local_cmd[1] = cmd_buffer[1];
                local_cmd[2] = cmd_buffer[2];

                __enable_irq();

                commandflag = 0;
                CommandHandler(local_cmd);
            }
            /* USER CODE END WHILE */
        }
    /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_11)
        HAL_DRDY_AdcCallback();
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI5)
        HAL_SPI_TX_AdcCallback();
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI5) return;

    uint32_t count = dma_transfer_count++;

    char msg[64];
    int len = snprintf(msg, sizeof(msg),
                       "DMA Transfer %lu Complete RX=%02X %02X %02X\r\n",
                       count,
                       ADC_Buffer.rx[0],
                       ADC_Buffer.rx[1],
                       ADC_Buffer.rx[2]);
    //HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, 100);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
        FFT_TxCompleteCallback();
}

/* ── CS toggle test ──────────────────────────────────────────────────────────
 * The board has a hardware inverter between the STM32 GPIO and the ADC CS pin.
 * So GPIO_PIN_SET  → ODR=1 → physical line LOW  → ADC CS active
 *    GPIO_PIN_RESET → ODR=0 → physical line HIGH → ADC CS inactive
 *
 * We print both ODR and physical IDR so the inversion is clearly visible.
 * ─────────────────────────────────────────────────────────────────────────── */
void Test_CS_Toggle(void)
{
    char buf[100];

    HAL_UART_Transmit(&huart1,
                      (uint8_t*)"CS TEST START\r\n",
                      15, HAL_MAX_DELAY);

    #define CS_ODR() ((ADC_SPI_CS1_GPIO_Port->ODR & ADC_SPI_CS1_Pin) ? 1u : 0u)
    #define CS_IDR() ((ADC_SPI_CS1_GPIO_Port->IDR & ADC_SPI_CS1_Pin) ? 1u : 0u)

    /* Initial state */
    snprintf(buf, sizeof(buf),
             "CS INITIAL   ODR=%u  IDR=%u\r\n", CS_ODR(), CS_IDR());
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

    /* Write SET */
    HAL_GPIO_WritePin(ADC_SPI_CS1_GPIO_Port, ADC_SPI_CS1_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    snprintf(buf, sizeof(buf),
             "After PIN_RESET   ODR=%u  IDR=%u\r\n", CS_ODR(), CS_IDR());
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

    /* Write RESET */
    HAL_GPIO_WritePin(ADC_SPI_CS1_GPIO_Port, ADC_SPI_CS1_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    snprintf(buf, sizeof(buf),
             "After PIN_SET ODR=%u  IDR=%u\r\n", CS_ODR(), CS_IDR());
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

    /* Leave CS in inactive state for your hardware:
     * If IDR=1 when ODR=1 → no inverter, use GPIO_PIN_SET for inactive
     * If IDR=0 when ODR=1 → inverter present, use GPIO_PIN_RESET for inactive */
    uint32_t odr_set = CS_ODR();   /* saved from PIN_SET state above? no — re-check */

    /* Drive SET and check IDR to determine inversion */
    HAL_GPIO_WritePin(ADC_SPI_CS1_GPIO_Port, ADC_SPI_CS1_Pin, GPIO_PIN_SET);
    HAL_Delay(5);
    uint32_t idr_when_set = CS_IDR();

    if (idr_when_set == 1)
    {
        /* No inverter: PIN_SET = line HIGH = CS inactive — leave as SET */
        HAL_UART_Transmit(&huart1,
            (uint8_t*)"CS config: no inverter. PIN_SET=inactive. Left HIGH.\r\n",
            54, HAL_MAX_DELAY);
    }
    else
    {
        /* Inverter present: PIN_SET = line LOW = CS active — switch to RESET */
        HAL_GPIO_WritePin(ADC_SPI_CS1_GPIO_Port, ADC_SPI_CS1_Pin, GPIO_PIN_RESET);
        HAL_UART_Transmit(&huart1,
            (uint8_t*)"CS config: INVERTER detected. PIN_RESET=inactive. Left LOW ODR.\r\n",
            65, HAL_MAX_DELAY);
    }

    #undef CS_ODR
    #undef CS_IDR

    HAL_UART_Transmit(&huart1,
                      (uint8_t*)"CS TEST DONE\r\n",
                      14, HAL_MAX_DELAY);
}

/* ── MISO test ───────────────────────────────────────────────────────────────
 * Run AFTER ADS_Init() so the ADC has been reset and is not driving MISO.
 * With CS held HIGH (inactive) the ADC tri-states its MISO output.
 * PULLDOWN should read 0, PULLUP should read 1.
 * ─────────────────────────────────────────────────────────────────────────── */
void Test_MISO(void)
{
    char buf[80];
    GPIO_PinState pin_state;

    HAL_UART_Transmit(&huart1, (uint8_t*)"MISO TEST START\r\n", 17, HAL_MAX_DELAY);

    /* Make sure CS is in inactive state so ADC tri-states MISO */
    HAL_GPIO_WritePin(ADC_SPI_CS1_GPIO_Port, ADC_SPI_CS1_Pin, GPIO_PIN_SET);
    HAL_Delay(5);

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Test 1: pulldown — free line should read 0 */
    GPIO_InitStruct.Pin  = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
    HAL_Delay(10);
    pin_state = HAL_GPIO_ReadPin(GPIOH, GPIO_PIN_8);
    snprintf(buf, sizeof(buf),
             "MISO PULLDOWN: %d (expect 0 if ADC tri-stated)\r\n", pin_state);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

    /* Test 2: pullup — free line should read 1 */
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
    HAL_Delay(10);
    pin_state = HAL_GPIO_ReadPin(GPIOH, GPIO_PIN_8);   /* read ONCE, check ONCE */
    snprintf(buf, sizeof(buf),
             "MISO PULLUP:   %d (expect 1 if ADC tri-stated)\r\n", pin_state);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

    /* Verdict based on the captured value */
    if (pin_state == GPIO_PIN_RESET)
    {
        HAL_UART_Transmit(&huart1,
            (uint8_t*)"WARNING: MISO stuck LOW — ADC driving line or hardware short\r\n",
            62, HAL_MAX_DELAY);
    }
    else
    {
        HAL_UART_Transmit(&huart1,
            (uint8_t*)"MISO OK — line is free (ADC tri-stated with CS inactive)\r\n",
            58, HAL_MAX_DELAY);
    }

    /* Restore MISO as SPI AF pin */
    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI5;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    HAL_UART_Transmit(&huart1, (uint8_t*)"MISO TEST DONE\r\n", 16, HAL_MAX_DELAY);
}
/* USER CODE END 4 */

/* ── Peripheral init ────────────────────────────────────────────────────── */

static void MX_GPDMA1_Init(void)
{
    __HAL_RCC_GPDMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel2_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel3_IRQn);
}

static void MX_SPI5_Init(void)
{
    hspi5.Instance                     = SPI5;
    hspi5.Init.Mode                    = SPI_MODE_MASTER;
    hspi5.Init.Direction               = SPI_DIRECTION_2LINES;
    hspi5.Init.DataSize                = SPI_DATASIZE_8BIT;
    hspi5.Init.CLKPolarity             = SPI_POLARITY_LOW;
    hspi5.Init.CLKPhase                = SPI_PHASE_2EDGE;
    hspi5.Init.NSS                     = SPI_NSS_SOFT;
    hspi5.Init.BaudRatePrescaler       = SPI_BAUDRATEPRESCALER_64;
    hspi5.Init.FirstBit                = SPI_FIRSTBIT_MSB;
    hspi5.Init.TIMode                  = SPI_TIMODE_DISABLE;
    hspi5.Init.CRCCalculation          = SPI_CRCCALCULATION_DISABLE;
    hspi5.Init.CRCPolynomial           = 0x7;
    hspi5.Init.NSSPMode                = SPI_NSS_PULSE_DISABLE;
    hspi5.Init.NSSPolarity             = SPI_NSS_POLARITY_LOW;
    hspi5.Init.FifoThreshold           = SPI_FIFO_THRESHOLD_01DATA;
    hspi5.Init.MasterSSIdleness        = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi5.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi5.Init.MasterReceiverAutoSusp  = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi5.Init.MasterKeepIOState       = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi5.Init.IOSwap                  = SPI_IO_SWAP_DISABLE;
    hspi5.Init.ReadyMasterManagement   = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
    hspi5.Init.ReadyPolarity           = SPI_RDY_POLARITY_HIGH;
    if (HAL_SPI_Init(&hspi5) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 576000;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_8;
    huart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)                          Error_Handler();
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    HAL_GPIO_WritePin(PSUEN_GPIO_Port,       PSUEN_Pin,       GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SYNC_RESET_GPIO_Port,  SYNC_RESET_Pin,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(ADC_SPI_CS1_GPIO_Port, ADC_SPI_CS1_Pin, GPIO_PIN_SET);

    HAL_EXTI_ConfigLineAttributes(EXTI_LINE_11, EXTI_LINE_SEC);

    GPIO_InitStruct.Pin  = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = PSUEN_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PSUEN_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SYNC_RESET_Pin;
    HAL_GPIO_Init(SYNC_RESET_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ADC_SPI_CS1_Pin;
    HAL_GPIO_Init(ADC_SPI_CS1_GPIO_Port, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI11_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI11_IRQn);
}

static void SystemIsolation_Config(void)
{
    __HAL_RCC_RIFSC_CLK_ENABLE();
    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel0,
            DMA_CHANNEL_SEC | DMA_CHANNEL_PRIV | DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();
    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel1,
            DMA_CHANNEL_SEC | DMA_CHANNEL_PRIV | DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();
    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel2,
            DMA_CHANNEL_SEC | DMA_CHANNEL_PRIV | DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();
    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel3,
            DMA_CHANNEL_SEC | DMA_CHANNEL_PRIV | DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
