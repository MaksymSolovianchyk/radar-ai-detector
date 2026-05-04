/* USER CODE BEGIN Header */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "radar.h"
#include "ads131m0x.h"
#include "radar_features.h"
#include "fft.h"
#include "sd_logger.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VECT_TAB_NS_OFFSET        0x00400
#define VTOR_TABLE_NS_START_ADDR  (SRAM2_AXI_BASE_NS | VECT_TAB_NS_OFFSET)
#define ADC_RESET_Pin             GPIO_PIN_3
#define ADC_RESET_GPIO_Port       GPIOB
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SD_HandleTypeDef   hsd2;
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
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void NonSecure_Init(void);
static void SystemIsolation_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_SPI5_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SDMMC2_SD_Init(void);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    HAL_Init();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    /* USER CODE BEGIN SysInit */
    /* USER CODE END SysInit */

    MX_GPIO_Init();
    MX_GPDMA1_Init();
    MX_SPI5_Init();
    MX_USART1_UART_Init();
    SystemIsolation_Config();   /* RIF must be configured before SD init */
    MX_SDMMC2_SD_Init();        /* soft-fail — radar runs even without SD */

    /* USER CODE BEGIN 2 */
    FFT_Init();
    ADS_Init();
    UartStartReceive();

    /* SD logger init — non-fatal */
    SD_Logger_Result sd_res = SD_Logger_Init();
    if (sd_res == SD_LOGGER_OK)
    {
        char msg[] = "SD logger ready.\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, sizeof(msg)-1, 200);
    }
    else
    {
        char msg[48];
        snprintf(msg, sizeof(msg), "SD unavailable (err=%d). UART only.\r\n", (int)sd_res);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 200);
    }
    /* USER CODE END 2 */

    //HAL_SuspendTick();
    //NonSecure_Init();

    /* USER CODE BEGIN WHILE */
    while (1)
    {
        if (FFT_IsReady())
        {
            FFT_Process();

            RadarFeatures_t rf;
            RadarFeatures_Compute(FFT_GetMagnitude(),
                                  FFT_N,
                                  (float)FFT_FS / (float)FFT_N,
                                  &rf);

            /* UART output — always */
            RadarFeatures_Print(&huart1, &rf);

            /* SD card output — only if logger is ready */
            if (SD_Logger_IsReady())
            {
                SD_Logger_Result wr = SD_Logger_Write(&rf);
                if (wr != SD_LOGGER_OK)
                {
                    SD_Logger_Flush();
                    char msg[] = "SD write error - logging stopped\r\n";
                    HAL_UART_Transmit(&huart1, (uint8_t*)msg, sizeof(msg)-1, 100);
                }
            }

            while (!FFT_TransmitDone()) { __NOP(); }
            FFT_Reset();
        }

        if (commandflag)
        {
            __disable_irq();
            uint8_t local_cmd[3];
            local_cmd[0] = cmd_buffer[0];
            local_cmd[1] = cmd_buffer[1];
            local_cmd[2] = cmd_buffer[2];
            __enable_irq();
            commandflag = 0;
            CommandHandler(local_cmd);
        }
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
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

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
        FFT_TxCompleteCallback();
}

/* USER CODE END 4 */

/* ── Peripheral init functions ─────────────────────────────────────────────── */

static void NonSecure_Init(void)
{
    funcptr_NS NonSecure_ResetHandler;
    SCB_NS->VTOR = VTOR_TABLE_NS_START_ADDR;
    __TZ_set_MSP_NS((*(uint32_t *)VTOR_TABLE_NS_START_ADDR));
    NonSecure_ResetHandler =
        (funcptr_NS)(*((uint32_t *)((VTOR_TABLE_NS_START_ADDR) + 4U)));
    NonSecure_ResetHandler();
}

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

static void MX_SDMMC2_SD_Init(void)
{
    hsd2.Instance                 = SDMMC2;
    hsd2.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd2.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd2.Init.BusWide             = SDMMC_BUS_WIDE_4B;
    hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd2.Init.ClockDiv            = 2;
    /* USER CODE BEGIN SDMMC2_Init 1 */
    if (HAL_SD_Init(&hsd2) != HAL_OK)
    {
        return;   /* soft-fail: no SD card inserted, radar keeps running */
    }
    /* USER CODE END SDMMC2_Init 1 */
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
    if (HAL_SPI_Init(&hspi5) != HAL_OK)
        Error_Handler();
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
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
        Error_Handler();
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
        Error_Handler();
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
        Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
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
    /* USER CODE BEGIN RIF_Init 0 */
    /* USER CODE END RIF_Init 0 */

    __HAL_RCC_RIFSC_CLK_ENABLE();

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel0,
            DMA_CHANNEL_SEC|DMA_CHANNEL_PRIV|
            DMA_CHANNEL_SRC_SEC|DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel1,
            DMA_CHANNEL_SEC|DMA_CHANNEL_PRIV|
            DMA_CHANNEL_SRC_SEC|DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel2,
            DMA_CHANNEL_SEC|DMA_CHANNEL_PRIV|
            DMA_CHANNEL_SRC_SEC|DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel3,
            DMA_CHANNEL_SEC|DMA_CHANNEL_PRIV|
            DMA_CHANNEL_SRC_SEC|DMA_CHANNEL_DEST_SEC) != HAL_OK)
        Error_Handler();

    /* USER CODE BEGIN RIF_Init 1 */
    /* USER CODE END RIF_Init 1 */

    /* USER CODE BEGIN RIF_Init 2 */
    /* USER CODE END RIF_Init 2 */
}

void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();
    while (1) {}
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
