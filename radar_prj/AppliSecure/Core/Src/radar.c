/*
 * radar.c
 *
 */

#include <string.h>
#include "ads131m0x.h"
#include "main.h"
#include <stdio.h>

//shuffle byte order since ads is big endian and stm32 arm is little endian
#define SHUFFLE(V)                      ((((V) & 0xFF000000) >> 24) | (((V) & 0x00FF0000) >> 8) | (((V) & 0x0000FF00) << 8) | (((V) & 0x000000FF) << 24))

// Convert a 24-bits 2-compliment number stored in a uin32_t to int32_t
#define INT24_TO_INT32(X) (int32_t)((uint32_t)(X) | (((uint32_t)(X) >> 23) * 0xFF000000L))

//ads word length conversion
#define NR_VALUES             6
#define VALUE_LENGTH_BITS     24
#define DIVIDE_ROUND_UP(N,D)  (((N) + (D) - 1) / (D))
#define DATA_LEN_UINT32       DIVIDE_ROUND_UP(NR_VALUES * VALUE_LENGTH_BITS, 32)

//Handles
extern UART_HandleTypeDef huart1;
//extern TIM_HandleTypeDef htim1;
//extern TIM_HandleTypeDef htim8;
//extern TIM_HandleTypeDef htim2;
extern SPI_HandleTypeDef hspi5;

//Variables
extern volatile bool spi_done;
volatile bool adcready = false;
volatile bool adc_data_ready = false;
volatile bool drdy_pulse = false;
volatile uint8_t cmd_buffer[3];
volatile int commandflag = 0;

ADC ADC_Buffer = {0};

uint8_t UartTxBuff[8] = {0}; //uint8_t UARTBuff[8] = {0};

uint8_t rxBuffer[3] = {0};


static inline uint32_t read_be24(const uint8_t *p)
{
    // combine 3 bytes (msb first) into a 24-bit value in the low bits of uint32_t
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

void ADS_Init(void)
{
	while(!adcready){
	uint16_t response = adcStartup() & 0xFF00;
	char buf[50];
		if (response == 0x0500){
			adcready = true;
			SETDCBLOCK(0x08);
		}
	HAL_Delay(1000);
	}
}

void ResetTXBuffer(ADC *adc) {
    memset(adc->tx, 0, sizeof(adc->tx));
}

void DrdyPulseCallback(void)
{
	drdy_pulse = true;
}

void DrdyExti(void)
{
	if (!adcready){
		return;
	}
	readData_DMA(&ADC_Buffer);
}


void BuffMemCopy(void){
	memcpy(UartTxBuff,&ADC_Buffer.rx[3],6);
}

void UartStartSend(void)
{
	UartTxBuff[6] = '\r';
	UartTxBuff[7] = '\n';
	HAL_UART_Transmit_DMA(&huart1, UartTxBuff, 8);
	//HAL_UART_Transmit(&huart1, (uint8_t*)UartTxBuff, 8, HAL_MAX_DELAY);
	memset(UartTxBuff, 0, 8);


}

void UartStartReceive(void){
	HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rxBuffer, sizeof(rxBuffer));
}

void UartRxcallback(void) {
    if (sizeof(rxBuffer) == 3) {
        cmd_buffer[0] = rxBuffer[0];
        cmd_buffer[1] = rxBuffer[1];
        cmd_buffer[2] = rxBuffer[2];
        commandflag = 1;
    }

    //UartStartReceive();
}


