/**
 * \copyright Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "main.h"
#include "ads131m0x.h"
#include "stm32n6xx_hal_spi.h"
#include "string.h"
#include "radar.h"

//Handles
//extern UART_HandleTypeDef huart1;
//extern TIM_HandleTypeDef htim1;
//extern TIM_HandleTypeDef htim8;
//extern TIM_HandleTypeDef htim2;

extern SPI_HandleTypeDef hspi5;

volatile bool spi_done = false;
volatile bool spi_tx_done = false;
//blocking dma spi function

volatile bool drdy_trigger = false;
//drdy trigger for setup

uint8_t spiRxBuf[18];

uint8_t CRC8Table[256] = {
	0x00, 0x97, 0xb9, 0x2e, 0xe5, 0x72, 0x5c, 0xcb,
	0x5d, 0xca, 0xe4, 0x73, 0xb8, 0x2f, 0x01, 0x96,
	0xba, 0x2d, 0x03, 0x94, 0x5f, 0xc8, 0xe6, 0x71,
	0xe7, 0x70, 0x5e, 0xc9, 0x02, 0x95, 0xbb, 0x2c,
	0xe3, 0x74, 0x5a, 0xcd, 0x06, 0x91, 0xbf, 0x28,
	0xbe, 0x29, 0x07, 0x90, 0x5b, 0xcc, 0xe2, 0x75,
	0x59, 0xce, 0xe0, 0x77, 0xbc, 0x2b, 0x05, 0x92,
	0x04, 0x93, 0xbd, 0x2a, 0xe1, 0x76, 0x58, 0xcf,
	0x51, 0xc6, 0xe8, 0x7f, 0xb4, 0x23, 0x0d, 0x9a,
	0x0c, 0x9b, 0xb5, 0x22, 0xe9, 0x7e, 0x50, 0xc7,
	0xeb, 0x7c, 0x52, 0xc5, 0x0e, 0x99, 0xb7, 0x20,
	0xb6, 0x21, 0x0f, 0x98, 0x53, 0xc4, 0xea, 0x7d,
	0xb2, 0x25, 0x0b, 0x9c, 0x57, 0xc0, 0xee, 0x79,
	0xef, 0x78, 0x56, 0xc1, 0x0a, 0x9d, 0xb3, 0x24,
	0x08, 0x9f, 0xb1, 0x26, 0xed, 0x7a, 0x54, 0xc3,
	0x55, 0xc2, 0xec, 0x7b, 0xb0, 0x27, 0x09, 0x9e,
	0xa2, 0x35, 0x1b, 0x8c, 0x47, 0xd0, 0xfe, 0x69,
	0xff, 0x68, 0x46, 0xd1, 0x1a, 0x8d, 0xa3, 0x34,
	0x18, 0x8f, 0xa1, 0x36, 0xfd, 0x6a, 0x44, 0xd3,
	0x45, 0xd2, 0xfc, 0x6b, 0xa0, 0x37, 0x19, 0x8e,
	0x41, 0xd6, 0xf8, 0x6f, 0xa4, 0x33, 0x1d, 0x8a,
	0x1c, 0x8b, 0xa5, 0x32, 0xf9, 0x6e, 0x40, 0xd7,
	0xfb, 0x6c, 0x42, 0xd5, 0x1e, 0x89, 0xa7, 0x30,
	0xa6, 0x31, 0x1f, 0x88, 0x43, 0xd4, 0xfa, 0x6d,
	0xf3, 0x64, 0x4a, 0xdd, 0x16, 0x81, 0xaf, 0x38,
	0xae, 0x39, 0x17, 0x80, 0x4b, 0xdc, 0xf2, 0x65,
	0x49, 0xde, 0xf0, 0x67, 0xac, 0x3b, 0x15, 0x82,
	0x14, 0x83, 0xad, 0x3a, 0xf1, 0x66, 0x48, 0xdf,
	0x10, 0x87, 0xa9, 0x3e, 0xf5, 0x62, 0x4c, 0xdb,
	0x4d, 0xda, 0xf4, 0x63, 0xa8, 0x3f, 0x11, 0x86,
	0xaa, 0x3d, 0x13, 0x84, 0x4f, 0xd8, 0xf6, 0x61,
	0xf7, 0x60, 0x4e, 0xd9, 0x12, 0x85, 0xab, 0x3c
};



//****************************************************************************
//
// Internal variables
//
//****************************************************************************

// Array used to recall device register map configurations */
static uint16_t             registerMap[NUM_REGISTERS];

// Array of SPI word lengths
const static uint8_t        wlength_byte_values[] = {2, 3, 4, 4};



//****************************************************************************
//
// Internal function prototypes
//
//****************************************************************************

uint8_t     buildSPIarray(const uint16_t opcodeArray[], uint8_t numberOpcodes, uint8_t byteArray[]);
uint16_t    enforce_selected_device_modes(uint16_t data);
uint8_t     getWordByteLength(void);


//spi done callback, used for data streaming
void HAL_SPI_AdcCallback(void)
{
    spi_done = true;

    /* Set the nCS pin HIGH */
}


//spi tx done callback, used for blocking transmissions
void HAL_SPI_TX_AdcCallback(void)
{
    spi_tx_done = true;

}

//spi done callback
void HAL_DRDY_AdcCallback(void)
{
    drdy_trigger = true;

}




//*****************************************************************************
//
//! Getter function to access registerMap array from outside of this module.
//!
//! \fn uint16_t getRegisterValue(uint8_t address)
//!
//! NOTE: The internal registerMap arrays stores the last know register value,
//! since the last read or write operation to that register. This function
//! does not communicate with the device to retrieve the current register value.
//! For the most up-to-date register data or retrieving the value of a hardware
//! controlled register, it is recommend to use readSingleRegister() to read the
//! current register value.
//!
//! \return unsigned 16-bit register value.
//
//*****************************************************************************
uint16_t getRegisterValue(uint8_t address)
{
    assert(address < NUM_REGISTERS);
    return registerMap[address];
}

void setCS(bool state)
{
    HAL_GPIO_WritePin(ADC_SPI_CS1_GPIO_Port,
                      ADC_SPI_CS1_Pin,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint16_t sendCommandDMA(uint16_t opcode)
{
	HAL_Delay(1);
	drdy_trigger = false;
	uint32_t startTick2 = HAL_GetTick();
	while (!drdy_trigger) {
		if ((HAL_GetTick()-startTick2) >1000){
			return 0xFFFF; //setup timeout
		}
	}
	spi_tx_done = false;
	(void)sendCommand(opcode);
	HAL_Delay(1);
    //wait for falling drdy at faster spi speeds
    drdy_trigger = false;
    uint32_t startTick = HAL_GetTick();
	while (!drdy_trigger) {
		if ((HAL_GetTick()-startTick) >1000){
			return 0xFFFF; //setup timeout
		}
	}
	//HAL_UART_Transmit(&huart1,(uint8_t*)"Im in return\r\n",15,100);
	return sendCommand(OPCODE_NULL);
}



//*****************************************************************************
//
//! Example start up sequence for the ADS131M0x.
//!
//! \fn void adcStartup(void)
//!
//! Before calling this function, the device must be powered,
//! the SPI/GPIO pins of the MCU must have already been configured,
//! and (if applicable) the external clock source should be provided to CLKIN.
//!
//! \return None.
//
//*****************************************************************************
uint16_t adcStartup(void)
{
	uint16_t response = 0x0000;
	HAL_GPIO_WritePin(PSUEN_GPIO_Port, PSUEN_Pin,	GPIO_PIN_SET);
	drdy_trigger = false;
	/* (OPTIONAL) Provide additional delay time for power supply settling */
	HAL_Delay(500);

	/* (REQUIRED) Set nRESET pin high for ADC operation */
	//setSYNC_RESET(SYNC_RESET_pin);
	HAL_GPIO_WritePin(SYNC_RESET_GPIO_Port, SYNC_RESET_Pin,	GPIO_PIN_SET);

	/* (OPTIONAL) Toggle nRESET pin to ensure default register settings. */
	/* NOTE: This also ensures that the device registers are unlocked.	 */
	//toggleRESET();
	HAL_GPIO_WritePin(SYNC_RESET_GPIO_Port, SYNC_RESET_Pin,	GPIO_PIN_RESET);
	HAL_Delay(500);
	HAL_GPIO_WritePin(SYNC_RESET_GPIO_Port, SYNC_RESET_Pin,	GPIO_PIN_SET);
	HAL_Delay(1000);

    /* (REQUIRED) Initialize internal 'registerMap' array with device default settings */
	restoreRegisterDefaults();

	//resetDevice(); //handle full duplex spi, discard first response
    /* (OPTIONAL) Validate first response word when beginning SPI communication: (0xFF20 | CHANCNT) */
	response = sendCommandDMA(OPCODE_NULL);
	//char buffer[50];
	//sprintf(buffer, "Response value: %u\r\n", response);

	//HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

	if (response == 0xFFFF){
		return 0xFFFF; //in case of error, define error response better

	}
	/* (OPTIONAL) Define your initial register settings here */
	if (writeSingleRegister(CLOCK_ADDRESS, (CLOCK_DEFAULT & ~CLOCK_OSR_MASK) | CLOCK_OSR_1024) == 0xFFFF){
    	return 0xFFFF;
    }

    sendCommandDMA(OPCODE_NULL);
    /* (REQUIRED) Configure MODE register settings
     * NOTE: This function call is required here for this particular code implementation to work.
     * This function will enforce
     * the MODE register settings as selected in the 'ads131m0x.h' header file.
     */
    if (writeSingleRegister(MODE_ADDRESS, MODE_DEFAULT) == 0xFFFF){
    	return 0xFFFF;
    }

	//short channels 2 and 3 to ground for diagnostic

	if (writeSingleRegister(CH2_CFG_ADDRESS, (CH2_CFG_DEFAULT & ~CH2_CFG_MUX2_MASK) | CH2_CFG_MUX2_ADC_INPUT_SHORT) == 0xFFFF) {
	    return 0xFFFF;
	}

	if (writeSingleRegister(CH3_CFG_ADDRESS, (CH3_CFG_DEFAULT & ~CH3_CFG_MUX3_MASK) | CH3_CFG_MUX3_ADC_INPUT_SHORT) == 0xFFFF) {
	    return 0xFFFF;
	}

	if (SETGAIN(0x0003) == 0xFFFF) {
		return 0xFFFF;
	}

	//autooffset calibration ch0 ch1
	if (AutoOffsetCH0CH1() == 0xFFFF) {
		return 0xFFFF;
	}


    /* (OPTIONAL) Read back all registers */

	/* (OPTIONAL) Check STATUS register for faults */


    /* (OPTIONAL) Validate first response word when beginning SPI communication: (0xFF20 | CHANCNT) */
    //get status
    return response;
}



//*****************************************************************************
//
//! Reads the contents of a single register at the specified address.
//!
//! \fn uint16_t readSingleRegister(uint8_t address)
//!
//! \param address is the 8-bit address of the register to read.
//!
//! \return Returns the 8-bit register read result.
//
//*****************************************************************************
uint16_t readSingleRegister(uint8_t address)
{
	/* Check that the register address is in range */
	assert(address < NUM_REGISTERS);

// Build TX and RX byte array
#ifdef ENABLE_CRC_IN
    uint8_t dataTx[8] = { 0 };      // 2 words, up to 4 bytes each = 8 bytes maximum
    uint8_t dataRx[8] = { 0 };
#else
    uint8_t dataTx[18] = { 0 };      // 1 word, up to 4 bytes long = 4 bytes maximum
    uint8_t dataRx[18] = { 0 };
#endif
    uint16_t opcode = OPCODE_RREG | (((uint16_t) address) << 7);
    uint8_t numberOfBytes = buildSPIarray(&opcode, 1, dataTx);

	/* Set the nCS pin LOW */
	setCS(LOW);

	spi_done = false;
	HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(&hspi5, dataTx, dataRx, numberOfBytes);
	if (status != HAL_OK) {
		setCS(HIGH);
		return 0xFFFF;
	}
	uint32_t startTick = HAL_GetTick();
	while (!spi_done) {
		if ((HAL_GetTick()-startTick) >1000){
			HAL_SPI_Abort(&hspi5);
			setCS(HIGH);
			return 0xFFFF;
		}
	}

	setCS(HIGH);

	// [FRAME 2] Send NULL command to retrieve the register data
	registerMap[address] = sendCommand(OPCODE_NULL);

	return registerMap[address];
}



//*****************************************************************************
//
//! Writes data to a single register.
//!
//! \fn void writeSingleRegister(uint8_t address, uint16_t data)
//!
//! \param address is the address of the register to write to.
//! \param data is the value to write.
//!
//! This command will be ignored if device registers are locked.
//!
//! \return None.
//
//*****************************************************************************
uint16_t writeSingleRegister(uint8_t address, uint16_t data)
{
    /* Check that the register address is in range */
    assert(address < NUM_REGISTERS);

    // (OPTIONAL) Enforce certain register field values when
    // writing to the MODE register to fix the operation mode
    if (MODE_ADDRESS == address)
    {
        data = enforce_selected_device_modes(data);
    }

    // Build TX and RX byte array
#ifdef ENABLE_CRC_IN
    uint8_t dataTx[12] = { 0 };     // 3 words, up to 4 bytes each = 12 bytes maximum
    uint8_t dataRx[12] = { 0 };
#else
    uint8_t dataTx[18] = { 0 };      // 2 words, up to 4 bytes long = 8 bytes maximum
    uint8_t dataRx[18] = { 0 };
#endif
    uint16_t opcodes[2];
    opcodes[0] = OPCODE_WREG | (((uint16_t) address) << 7);
    opcodes[1] = data;
    uint8_t numberOfBytes = buildSPIarray(&opcodes[0], 2, dataTx);

    //build comm frame
    numberOfBytes = 18;
    dataTx[4]=dataTx[3];
    dataTx[3]=dataTx[2];
    dataTx[2]=0;
    int i;
    for (i=5; i<numberOfBytes; i++){
    	dataTx[i]=0;
    }
    //block till falling edge to ensure process is complete before advancing
    drdy_trigger = false;
    uint32_t starTick2 = HAL_GetTick();
	while (!drdy_trigger) {
		HAL_Delay(10);
		if ((HAL_GetTick()-starTick2) >3000){
			return 0xFFFF; //setup timeout
		}
	}

    //check for busy
    uint32_t PreStartick = HAL_GetTick();
    while (hspi5.State != HAL_SPI_STATE_READY) {
        if ((HAL_GetTick() - PreStartick) > 1000) {
            setCS(HIGH);
            return 0xFFFF;
        }
        __NOP();
    }

    // Send command
    //spiSendReceiveArrays(dataTx, dataRx, numberOfBytes);
    setCS(LOW);
    spi_done = false;
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(&hspi5, dataTx, dataRx, numberOfBytes);
    if (status != HAL_OK)
    {
        setCS(HIGH);
        return 0xFFFF;
    }

    uint32_t startTick = HAL_GetTick();
    while (!spi_done)
    {
        if ((HAL_GetTick() - startTick) > 1000)
        {
            HAL_SPI_Abort(&hspi5);  // optional
            setCS(HIGH);
            return 0xFFFF;  // timeout
        }
    }

    setCS(HIGH);
    // Update internal array
    registerMap[address] = data;

    //block till falling edge to ensure process is complete before advancing
    drdy_trigger = false;
    uint32_t starTick = HAL_GetTick();
	while (!drdy_trigger) {
		if ((HAL_GetTick()-starTick) >3000){
			return 0xFFFF; //setup timeout
		}
	}
    // (RECOMMENDED) Read back register to confirm register write was successful
    //readSingleRegister(address);
    // NOTE: Enabling the CRC words in the SPI command will NOT prevent an invalid W
    return 0x0000;
}

//Dial functions
uint16_t SETGAIN(uint16_t GAIN_SETTING)
{
	if (GAIN_SETTING > 7) {return 0xFFFF;} //improper gain setting
	if (writeSingleRegister(GAIN1_ADDRESS, GAIN_SETTING | GAIN_SETTING << 4) == 0xFFFF) {return 0xFFFF;}
	return 0x0000;
}

uint16_t SETCLOCK(uint16_t CLOCK_SETTING)
{
	if (CLOCK_SETTING > 7) {return 0xFFFF;} //improper clock setting
	if (writeSingleRegister(CLOCK_ADDRESS, (CLOCK_DEFAULT & ~CLOCK_OSR_MASK) | CLOCK_SETTING << 2 ) == 0xFFFF){return 0xFFFF;}
	return 0x0000;
}

uint16_t SETDCBLOCK(uint16_t DCBLOCK_SETTING)
{
	if (DCBLOCK_SETTING > 15) {return 0xFFFF;} //improper dcblock setting
	if (writeSingleRegister(THRSHLD_LSB_ADDRESS, DCBLOCK_SETTING) == 0xFFFF) {return 0xFFFF;}
	return 0x0000;
}

uint16_t CH0OFFSET(uint32_t CH0_OCAL)
{
	if ((CH0_OCAL & 0xFF)!= 0x00) {return 0xFFFF;} //improper 24bit offset
	if (writeSingleRegister(CH0_OCAL_MSB_ADDRESS, CH0_OCAL >> 16) == 0xFFFF) {return 0xFFFF;}
	if (writeSingleRegister(CH0_OCAL_LSB_ADDRESS, CH0_OCAL) == 0xFFFF) {return 0xFFFF;}
	return 0x0000;
}

uint16_t CH1OFFSET(uint32_t CH1_OCAL)
{
	if ((CH1_OCAL & 0xFF)!= 0x00) {return 0xFFFF;} //improper 24bit offset
	if (writeSingleRegister(CH1_OCAL_MSB_ADDRESS, CH1_OCAL >> 16) == 0xFFFF) {return 0xFFFF;}
	if (writeSingleRegister(CH1_OCAL_LSB_ADDRESS, CH1_OCAL) == 0xFFFF) {return 0xFFFF;}
	return 0x0000;
}

uint16_t CH0GAIN(uint32_t CH0_GAIN)
{
	if ((CH0_GAIN & 0xFF)!= 0x00) {return 0xFFFF;} //improper 24bit gain
	if (writeSingleRegister(CH0_GCAL_MSB_ADDRESS, CH0_GAIN >> 16) == 0xFFFF) {return 0xFFFF;}
	if (writeSingleRegister(CH0_GCAL_LSB_ADDRESS, CH0_GAIN) == 0xFFFF) {return 0xFFFF;}
	return 0x0000;
}

uint16_t CH1GAIN(uint32_t CH1_GAIN)
{
	if ((CH1_GAIN & 0xFF)!= 0x00) {return 0xFFFF;} //improper 24bit gain
	if (writeSingleRegister(CH1_GCAL_MSB_ADDRESS, CH1_GAIN >> 16) == 0xFFFF) {return 0xFFFF;}
	if (writeSingleRegister(CH1_GCAL_LSB_ADDRESS, CH1_GAIN) == 0xFFFF) {return 0xFFFF;}
	return 0x0000;
}

uint16_t AutoOffsetCH0CH1(void){
	int sampleSize = 5;
	int i = 0;
	ADC ADC_offsetbuff = {0};
	int32_t CH0sum = 0;
	int32_t CH1sum = 0;
	//short adc input ch0 ch1 for calibration
	if (writeSingleRegister(CH0_CFG_ADDRESS, (CH0_CFG_DEFAULT & ~CH0_CFG_MUX0_MASK) | CH0_CFG_MUX0_ADC_INPUT_SHORT) == 0xFFFF) {
	    return 0xFFFF;
	}
	if (writeSingleRegister(CH1_CFG_ADDRESS, (CH1_CFG_DEFAULT & ~CH1_CFG_MUX1_MASK) | CH1_CFG_MUX1_ADC_INPUT_SHORT) == 0xFFFF) {
	    return 0xFFFF;
	}

	for (i = 0; i < sampleSize; i++){
		spi_done = false;
		readData_DMA(&ADC_offsetbuff);
		uint32_t startTick = HAL_GetTick();
		while (!spi_done) {
			if ((HAL_GetTick()-startTick) >1000){
				return 0xFFFF; //setup timeout
			}
		}
		int32_t CH0temp = INT24_TO_INT32((ADC_offsetbuff.rx[3] << 16) | (ADC_offsetbuff.rx[4] << 8) | ADC_offsetbuff.rx[5]);
		int32_t CH1temp = INT24_TO_INT32((ADC_offsetbuff.rx[6] << 16) | (ADC_offsetbuff.rx[7] << 8) | ADC_offsetbuff.rx[8]);
		CH0sum += CH0temp;
		CH1sum += CH1temp;
	}

	int32_t CH0avg = (int32_t)(CH0sum / sampleSize);
	int32_t CH1avg = (int32_t)(CH1sum / sampleSize);

	uint32_t CH0_OCAL = ((uint32_t)CH0avg & 0xFFFFFFu) << 8;
	uint32_t CH1_OCAL = ((uint32_t)CH1avg & 0xFFFFFFu) << 8;

	uint16_t retval1 = CH0OFFSET(CH0_OCAL);
	uint16_t retval2 = CH1OFFSET(CH1_OCAL);
	//unshort adc input ch0 ch1
	if (writeSingleRegister(CH0_CFG_ADDRESS, (CH0_CFG_DEFAULT & ~CH0_CFG_MUX0_MASK) | CH0_CFG_MUX0_AIN0P_AIN0N) == 0xFFFF) {
	    return 0xFFFF;
	}
	if (writeSingleRegister(CH1_CFG_ADDRESS, (CH1_CFG_DEFAULT & ~CH1_CFG_MUX1_MASK) | CH1_CFG_MUX1_AIN1P_AIN1N) == 0xFFFF) {
	    return 0xFFFF;
	}

	if (retval1 || retval2){
		return 0xFFFF;
	}
	return 0x0000;
}

uint8_t crc8(uint8_t *msg, int lengthOfMsg, uint8_t init) {
  while (lengthOfMsg-- > 0) init = CRC8Table[ (init ^ *msg++)];
  return init;
};


void CommandHandler(uint8_t *cData) {
    if (!cData) return;
    uint8_t computed_crc = crc8(cData, 2, 0x00);
    uint8_t received_crc = cData[2];  // CRC is at index 2

    if (computed_crc != received_crc) {
        return;
    }

    uint8_t byte0 = cData[0];
    uint8_t byte1 = cData[1];


    switch (byte0) {
    	case 0x01:
    		SETGAIN(byte1);
    		break;
        case 0x02:
        	SETCLOCK(byte1);
            break;
        case 0x03:
        	SETDCBLOCK(byte1);
            break;
        case 0x04:
        	AutoOffsetCH0CH1();
//			HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_2);
//			HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_1);
//			HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
//			// reset counter
//			__HAL_TIM_SET_COUNTER(&htim2, 0);
//
//			// enable preload for both channels
//			TIM2->CCMR1 |= TIM_CCMR1_OC1PE;
//			TIM2->EGR   |= TIM_EGR_UG;  // force update, latch CCRs
//
//			// enable CH1 and CH2N outputs
//			TIM2->CCER |= TIM_CCER_CC1E;
//
//			// enable main output (advanced-control timers need this)
//			TIM2->BDTR |= TIM_BDTR_MOE;
//
//			// start timer in one-pulse mode
//			TIM2->CR1 |= TIM_CR1_CEN;

            break;
        default:
            break;
    }
}



//*****************************************************************************
//
//! Reads ADC data.
//!
//! \fn bool readData(adc_channel_data *DataStruct)
//!
//! \param *DataStruct points to an adc_channel_data type-defined structure/
//!
//! NOTE: Should be called after /DRDY goes low, and not during a /DRDY falling edge!
//!
//! \return Returns true if the CRC-OUT of the data read detects an error.
//
//*****************************************************************************
bool readData(adc_channel_data *DataStruct)
{
    int i;
    uint8_t crcTx[4]                        = { 0 };
    uint8_t dataRx[4]                       = { 0 };
    uint8_t bytesPerWord                    = getWordByteLength();

#ifdef ENABLE_CRC_IN
    // Build CRC word (only if "RX_CRC_EN" register bit is enabled)
    uint16_t crcWordIn = calculateCRC(&DataTx[0], bytesPerWord * 2, 0xFFFF);
    crcTx[0] = upperByte(crcWordIn);
    crcTx[1] = lowerByte(crcWordIn);
#endif

    /* Set the nCS pin LOW */
    setCS(LOW);

    // Send NULL word, receive response word
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->response = combineBytes(dataRx[0], dataRx[1]);

    // (OPTIONAL) Do something with the response (STATUS) word.
    // ...Here we only use the response for calculating the CRC-OUT
    //uint16_t crcWord = calculateCRC(&dataRx[0], bytesPerWord, 0xFFFF);

    // (OPTIONAL) Ignore CRC error checking
    uint16_t crcWord = 0;

    // Send 2nd word, receive channel 1 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(crcTx[i]);
    }
    DataStruct->channel0 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#if (CHANNEL_COUNT > 1)

    // Send 3rd word, receive channel 2 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->channel1 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#endif
#if (CHANNEL_COUNT > 2)

    // Send 4th word, receive channel 3 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->channel2 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#endif
#if (CHANNEL_COUNT > 3)

    // Send 5th word, receive channel 4 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->channel3 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#endif
#if (CHANNEL_COUNT > 4)

    // Send 6th word, receive channel 5 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->channel4 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#endif
#if (CHANNEL_COUNT > 5)

    // Send 7th word, receive channel 6 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->channel5 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#endif
#if (CHANNEL_COUNT > 6)

    // Send 8th word, receive channel 7 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->channel6 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#endif
#if (CHANNEL_COUNT > 7)

    // Send 9th word, receive channel 8 data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->channel7 = signExtend(&dataRx[0]);
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

#endif

    // Send the next word, receive CRC data
    for (i = 0; i < bytesPerWord; i++)
    {
        dataRx[i] = spiSendReceiveByte(0x00);
    }
    DataStruct->crc = combineBytes(dataRx[0], dataRx[1]);

    /* NOTE: If we continue calculating the CRC with a matching CRC, the result should be zero.
     * Any non-zero result will indicate a mismatch.
     */
    //crcWord = calculateCRC(&dataRx[0], bytesPerWord, crcWord);

    /* Set the nCS pin HIGH */
    setCS(HIGH);

    // Returns true when a CRC error occurs
    return ((bool) crcWord);
}

bool readData_DMA(ADC *ADC_Buffer)
{
//    uint32_t PreStartick = HAL_GetTick();
//    while (hspi2.State != HAL_SPI_STATE_READY) {
//        if ((HAL_GetTick() - PreStartick) > 1000) {
//            setCS(HIGH);
//            return false;
//        }
//        __NOP();
//    }

    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(&hspi5, ADC_Buffer->tx, ADC_Buffer->rx, 18);
    if (status != HAL_OK)
    {
        return false;
    }
    return true; //no hall error
}


//*****************************************************************************
//
//! Sends the specified SPI command to the ADC (NULL, STANDBY, or WAKEUP).
//!
//! \fn uint16_t sendCommand(uint16_t opcode)
//!
//! \param opcode SPI command byte.
//!
//! NOTE: Other commands have their own dedicated functions to support
//! additional functionality.
//!
//! \return ADC response byte (typically the STATUS byte).
//
//*****************************************************************************
uint16_t sendCommand(uint16_t opcode)
{
    /* Assert if this function is used to send any of the following opcodes */
    assert(OPCODE_RREG != opcode);      /* Use "readSingleRegister()"   */
    assert(OPCODE_WREG != opcode);      /* Use "writeSingleRegister()"  */
    assert(OPCODE_LOCK != opcode);      /* Use "lockRegisters()"        */
    assert(OPCODE_UNLOCK != opcode);    /* Use "unlockRegisters()"      */
    assert(OPCODE_RESET != opcode);     /* Use "resetDevice()"          */

    // Build TX and RX byte array
#ifdef ENABLE_CRC_IN
    static uint8_t dataTx[8] = { 0 };      // 2 words, up to 4 bytes each = 8 bytes maximum
    static uint8_t dataRx[8] = { 0 };
#else
    static uint8_t dataTx[18] = { 0 };      // 1 word, up to 4 bytes long = 4 bytes maximum
    static uint8_t dataRx[18] = { 0 };
#endif
    uint8_t numberOfBytes = 18;//buildSPIarray(&opcode, 1, dataTx);

    dataTx[0] = (opcode >> 8) & 0xFF;
    dataTx[1] = opcode & 0xFF;

    /* Set the nCS pin LOW */
    setCS(LOW);

    uint32_t PreStartick = HAL_GetTick();
    while (hspi5.State != HAL_SPI_STATE_READY) {

        if ((HAL_GetTick() - PreStartick) > 1000) {
            setCS(HIGH);
            return 0xFFFF;
        }
        __NOP();
    }
	spi_done = false;
    HAL_StatusTypeDef statustx = HAL_SPI_TransmitReceive_DMA(&hspi5, dataTx, dataRx, numberOfBytes); //sendcommand
    if (statustx != HAL_OK) {
        setCS(HIGH);
        return 0xFFFF;
    }

    uint32_t PostStartick = HAL_GetTick();
    while (HAL_SPI_GetState(&hspi5) == HAL_SPI_STATE_BUSY_TX) {
        if (HAL_GetTick() - PostStartick > 100) { // 10ms timeout
            HAL_SPI_Abort(&hspi5);
            return 0xFFFF;
        }
        __NOP();
    }

/*    uint32_t startTick = HAL_GetTick();
    while (!spi_tx_done) {
       if ((HAL_GetTick()-startTick) >1000){
    	   HAL_UART_Transmit(&huart1,(uint8_t*)"return shit3 \r\n",20,100);
       	HAL_SPI_Abort(&hspi5);
       	setCS(HIGH);
       	return 0xFFFF;
        }
   }
   */
    //for setup getting the right message really matters, workaround

    uint32_t startTick = HAL_GetTick();
	while (!spi_done) {
		if ((HAL_GetTick()-startTick) >1000){
			return 0xFFFF; //failure
		}
	}

    setCS(HIGH);
   // HAL_UART_Transmit(&huart1,(uint8_t*)"return NOT trash \r\n",20,100);
    return combineBytes(dataRx[0], dataRx[1]);
}



//*****************************************************************************
//
//! Resets the device.
//!
//! \fn void resetDevice(void)
//!
//! NOTE: This function does not capture DOUT data, but it could be modified
//! to do so.
//!
//! \return None.
//
//*****************************************************************************
void resetDevice(void)
{
    // Build TX and RX byte array
#ifdef ENABLE_CRC_IN
    uint8_t dataTx[8] = { 0 };      // 2 words, up to 4 bytes each = 8 bytes maximum
    //uint8_t dataRx[8] = { 0 };    // Only needed if capturing data
#else
    uint8_t dataTx[18] = { 0 };      // 1 word, up to 4 bytes long = 4 bytes maximum
    uint8_t dataRx[18] = { 0 };
#endif
    uint16_t opcode         = OPCODE_RESET;
    uint8_t numberOfBytes   = 18;//buildSPIarray(&opcode, 1, dataTx);

    dataTx[0] = (opcode >> 8) & 0xFF;
    dataTx[1] = opcode & 0xFF;

//    uint8_t bytesPerWord    = wlength_byte_values[WLENGTH];
//    uint8_t wordsInFrame    = CHANNEL_COUNT + 2;

    /* Set the nCS pin LOW */
    setCS(LOW);

    spi_done = false;
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(&hspi5, dataTx, dataRx, numberOfBytes);
    if (status != HAL_OK) {
        setCS(HIGH);
    }
    uint32_t startTick = HAL_GetTick();
    while (!spi_done) {
        if ((HAL_GetTick()-startTick) >1000){
        	//HAL_SPI_Abort(&hspi2);
        	setCS(HIGH);
        }
    }

    setCS(HIGH);

    // tSRLRST delay, ~1ms with 2.048 MHz fCLK
    //HAL_Delay(3);

    // Update register setting array to keep software in sync with device
    restoreRegisterDefaults();

    // Write to MODE register to enforce mode settings
    //writeSingleRegister(MODE_ADDRESS, MODE_DEFAULT);
}



//*****************************************************************************
//
//! Sends the LOCK command and verifies that registers are locked.
//!
//! \fn bool lockRegisters(void)
//!
//! \return boolean to indicate if an error occurred (0 = no error; 1 = error)
//
//*****************************************************************************
bool lockRegisters(void)
{
    bool b_lock_error;

    // Build TX and RX byte array
#ifdef ENABLE_CRC_IN
    uint8_t dataTx[8] = { 0 };      // 2 words, up to 4 bytes each = 8 bytes maximum
    uint8_t dataRx[8] = { 0 };
#else
    uint8_t dataTx[4] = { 0 };      // 1 word, up to 4 bytes long = 4 bytes maximum
    uint8_t dataRx[4] = { 0 };
#endif
    uint16_t opcode         = OPCODE_LOCK;
    uint8_t numberOfBytes   = buildSPIarray(&opcode, 1, dataTx);

    // Send command
	/* Set the nCS pin LOW */
	setCS(LOW);

	spi_done = false;
	HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(&hspi5, dataTx, dataRx, numberOfBytes);
	if (status != HAL_OK) {
		setCS(HIGH);
		return 0xFFFF;
	}
	uint32_t startTick = HAL_GetTick();
	while (!spi_done) {
		if ((HAL_GetTick()-startTick) >1000){
			HAL_SPI_Abort(&hspi5);
			setCS(HIGH);
			return 0xFFFF;
		}
	}

	setCS(HIGH);

    /* (OPTIONAL) Check for SPI errors by sending the NULL command and checking STATUS */

    /* (OPTIONAL) Read back the STATUS register and check if LOCK bit is set... */
    readSingleRegister(STATUS_ADDRESS);
    if (!SPI_LOCKED) { b_lock_error = true; }

    /* If the STATUS register is NOT read back,
     * then make sure to manually update the global register map variable... */
    //registerMap[STATUS_ADDRESS]  |= STATUS_LOCK_LOCKED;

    /* (OPTIONAL) Error handler */
    if (b_lock_error)
    {
        // Insert error handler function call here...
    }

    return b_lock_error;
}



//*****************************************************************************
//
//! Sends the UNLOCK command and verifies that registers are unlocked
//!
//! \fn bool unlockRegisters(void)
//!
//! \return boolean to indicate if an error occurred (0 = no error; 1 = error)
//
//*****************************************************************************
bool unlockRegisters(void)
{
	bool b_unlock_error;

    // Build TX and RX byte array
#ifdef ENABLE_CRC_IN
    uint8_t dataTx[8] = { 0 };      // 2 words, up to 4 bytes each = 8 bytes maximum
    uint8_t dataRx[8] = { 0 };
#else
    uint8_t dataTx[4] = { 0 };      // 1 word, up to 4 bytes long = 4 bytes maximum
    uint8_t dataRx[4] = { 0 };
#endif
    uint16_t opcode = OPCODE_UNLOCK;
    uint8_t numberOfBytes = buildSPIarray(&opcode, 1, dataTx);

    // Send command
	/* Set the nCS pin LOW */
	setCS(LOW);

	spi_done = false;
	HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(&hspi5, dataTx, dataRx, numberOfBytes);
	if (status != HAL_OK) {
		setCS(HIGH);
		return 0xFFFF;
	}
	uint32_t startTick = HAL_GetTick();
	while (!spi_done) {
		if ((HAL_GetTick()-startTick) >1000){
			HAL_SPI_Abort(&hspi5);
			setCS(HIGH);
			return 0xFFFF;
		}
	}

	setCS(HIGH);

    /* (OPTIONAL) Check for SPI errors by sending the NULL command and checking STATUS */

    /* (OPTIONAL) Read the STATUS register and check if LOCK bit is cleared... */
    readSingleRegister(STATUS_ADDRESS);
    if (SPI_LOCKED) { b_unlock_error = true; }

    /* If the STATUS register is NOT read back,
     * then make sure to manually update the global register map variable... */
    //registerMap[STATUS_ADDRESS]  &= !STATUS_LOCK_LOCKED;

    /* (OPTIONAL) Error handler */
    if (b_unlock_error)
    {
        // Insert error handler function call here...
    }

    return b_unlock_error;
}



//*****************************************************************************
//
//! Calculates the 16-bit CRC for the selected CRC polynomial.
//!
//! \fn uint16_t calculateCRC(const uint8_t dataBytes[], uint8_t numberBytes, uint16_t initialValue)
//!
//! \param dataBytes[] pointer to first element in the data byte array
//! \param numberBytes number of bytes to be used in CRC calculation
//! \param initialValue the seed value (or partial crc calculation), use 0xFFFF when beginning a new CRC computation
//!
//! NOTE: This calculation is shown as an example and is not optimized for speed.
//!
//! \return 16-bit calculated CRC word
//
//*****************************************************************************
uint16_t calculateCRC(const uint8_t dataBytes[], uint8_t numberBytes, uint16_t initialValue)
{
	/* Check that "dataBytes" is not a null pointer */
	assert(dataBytes != 0x00);

	int         bitIndex, byteIndex;
	bool        dataMSb;						/* Most significant bit of data byte */
	bool        crcMSb;						    /* Most significant bit of crc byte  */
	uint8_t     bytesPerWord = wlength_byte_values[WLENGTH];

	/*
     * Initial value of crc register
     * NOTE: The ADS131M0x defaults to 0xFFFF,
     * but can be set at function call to continue an on-going calculation
     */
    uint16_t crc = initialValue;

    #ifdef CRC_CCITT
    /* CCITT CRC polynomial = x^16 + x^12 + x^5 + 1 */
    const uint16_t poly = 0x1021;
    #endif

    #ifdef CRC_ANSI
    /* ANSI CRC polynomial = x^16 + x^15 + x^2 + 1 */
    const uint16_t poly = 0x8005;
    #endif

    //
    // CRC algorithm
    //

    // Loop through all bytes in the dataBytes[] array
	for (byteIndex = 0; byteIndex < numberBytes; byteIndex++)
	{
	    // Point to MSb in byte
	    bitIndex = 0x80u;

	    // Loop through all bits in the current byte
	    while (bitIndex > 0)
	    {
	        // Check MSB's of data and crc
	        dataMSb = (bool) (dataBytes[byteIndex] & bitIndex);
	        crcMSb  = (bool) (crc & 0x8000u);

	        crc <<= 1;              /* Left shift CRC register */

	        // Check if XOR operation of MSBs results in additional XOR operations
	        if (dataMSb ^ crcMSb)
	        {
	            crc ^= poly;        /* XOR crc with polynomial */
	        }

	        /* Shift MSb pointer to the next data bit */
	        bitIndex >>= 1;
	    }
	}

	return crc;
}



//*****************************************************************************
//
//! Updates the registerMap[] array to its default values.
//!
//! \fn void restoreRegisterDefaults(void)
//!
//! NOTES:
//! - If the MCU keeps a copy of the ADS131M0x register settings in memory,
//! then it is important to ensure that these values remain in sync with the
//! actual hardware settings. In order to help facilitate this, this function
//! should be called after powering up or resetting the device (either by
//! hardware pin control or SPI software command).
//!
//! - Reading back all of the registers after resetting the device can
//! accomplish the same result; however, this might be problematic if the
//! device was previously in CRC mode or the WLENGTH was modified, since
//! resetting the device exits these modes. If the MCU is not aware of this
//! mode change, then read register commands will return invalid data due to
//! the expectation of data appearing in a different byte position.
//!
//! \return None.
//
//*****************************************************************************
void restoreRegisterDefaults(void)
{
    registerMap[ID_ADDRESS]             =   0x00;               /* NOTE: This a read-only register */
    registerMap[STATUS_ADDRESS]         =   STATUS_DEFAULT;
    registerMap[MODE_ADDRESS]           =   MODE_DEFAULT;
    registerMap[CLOCK_ADDRESS]          =   CLOCK_DEFAULT;
    registerMap[GAIN1_ADDRESS]          =   GAIN1_DEFAULT;
    registerMap[GAIN2_ADDRESS]          =   GAIN2_DEFAULT;
    registerMap[CFG_ADDRESS]            =   CFG_DEFAULT;
    registerMap[THRSHLD_MSB_ADDRESS]    =   THRSHLD_MSB_DEFAULT;
    registerMap[THRSHLD_LSB_ADDRESS]    =   THRSHLD_LSB_DEFAULT;
    registerMap[CH0_CFG_ADDRESS]        =   CH0_CFG_DEFAULT;
    registerMap[CH0_OCAL_MSB_ADDRESS]   =   CH0_OCAL_MSB_DEFAULT;
    registerMap[CH0_OCAL_LSB_ADDRESS]   =   CH0_OCAL_LSB_DEFAULT;
    registerMap[CH0_GCAL_MSB_ADDRESS]   =   CH0_GCAL_MSB_DEFAULT;
    registerMap[CH0_GCAL_LSB_ADDRESS]   =   CH0_GCAL_LSB_DEFAULT;
#if (CHANNEL_COUNT > 1)
    registerMap[CH1_CFG_ADDRESS]        =   CH1_CFG_DEFAULT;
    registerMap[CH1_OCAL_MSB_ADDRESS]   =   CH1_OCAL_MSB_DEFAULT;
    registerMap[CH1_OCAL_LSB_ADDRESS]   =   CH1_OCAL_LSB_DEFAULT;
    registerMap[CH1_GCAL_MSB_ADDRESS]   =   CH1_GCAL_MSB_DEFAULT;
    registerMap[CH1_GCAL_LSB_ADDRESS]   =   CH1_GCAL_LSB_DEFAULT;
#endif
#if (CHANNEL_COUNT > 2)
    registerMap[CH2_CFG_ADDRESS]        =   CH2_CFG_DEFAULT;
    registerMap[CH2_OCAL_MSB_ADDRESS]   =   CH2_OCAL_MSB_DEFAULT;
    registerMap[CH2_OCAL_LSB_ADDRESS]   =   CH2_OCAL_LSB_DEFAULT;
    registerMap[CH2_GCAL_MSB_ADDRESS]   =   CH2_GCAL_MSB_DEFAULT;
    registerMap[CH2_GCAL_LSB_ADDRESS]   =   CH2_GCAL_LSB_DEFAULT;
#endif
#if (CHANNEL_COUNT > 3)
    registerMap[CH3_CFG_ADDRESS]        =   CH3_CFG_DEFAULT;
    registerMap[CH3_OCAL_MSB_ADDRESS]   =   CH3_OCAL_MSB_DEFAULT;
    registerMap[CH3_OCAL_LSB_ADDRESS]   =   CH3_OCAL_LSB_DEFAULT;
    registerMap[CH3_GCAL_MSB_ADDRESS]   =   CH3_GCAL_MSB_DEFAULT;
    registerMap[CH3_GCAL_LSB_ADDRESS]   =   CH3_GCAL_LSB_DEFAULT;
#endif
#if (CHANNEL_COUNT > 4)
    registerMap[CH4_CFG_ADDRESS]        =   CH4_CFG_DEFAULT;
    registerMap[CH4_OCAL_MSB_ADDRESS]   =   CH4_OCAL_MSB_DEFAULT;
    registerMap[CH4_OCAL_LSB_ADDRESS]   =   CH4_OCAL_LSB_DEFAULT;
    registerMap[CH4_GCAL_MSB_ADDRESS]   =   CH4_GCAL_MSB_DEFAULT;
    registerMap[CH4_GCAL_LSB_ADDRESS]   =   CH4_GCAL_LSB_DEFAULT;
#endif
#if (CHANNEL_COUNT > 5)
    registerMap[CH5_CFG_ADDRESS]        =   CH5_CFG_DEFAULT;
    registerMap[CH5_OCAL_MSB_ADDRESS]   =   CH5_OCAL_MSB_DEFAULT;
    registerMap[CH5_OCAL_LSB_ADDRESS]   =   CH5_OCAL_LSB_DEFAULT;
    registerMap[CH5_GCAL_MSB_ADDRESS]   =   CH5_GCAL_MSB_DEFAULT;
    registerMap[CH5_GCAL_LSB_ADDRESS]   =   CH5_GCAL_LSB_DEFAULT;
#endif
#if (CHANNEL_COUNT > 6)
    registerMap[CH6_CFG_ADDRESS]        =   CH6_CFG_DEFAULT;
    registerMap[CH6_OCAL_MSB_ADDRESS]   =   CH6_OCAL_MSB_DEFAULT;
    registerMap[CH6_OCAL_LSB_ADDRESS]   =   CH6_OCAL_LSB_DEFAULT;
    registerMap[CH6_GCAL_MSB_ADDRESS]   =   CH6_GCAL_MSB_DEFAULT;
    registerMap[CH6_GCAL_LSB_ADDRESS]   =   CH6_GCAL_LSB_DEFAULT;
#endif
#if (CHANNEL_COUNT > 7)
    registerMap[CH7_CFG_ADDRESS]        =   CH7_CFG_DEFAULT;
    registerMap[CH7_OCAL_MSB_ADDRESS]   =   CH7_OCAL_MSB_DEFAULT;
    registerMap[CH7_OCAL_LSB_ADDRESS]   =   CH7_OCAL_LSB_DEFAULT;
    registerMap[CH7_GCAL_MSB_ADDRESS]   =   CH7_GCAL_MSB_DEFAULT;
    registerMap[CH7_GCAL_LSB_ADDRESS]   =   CH7_GCAL_LSB_DEFAULT;
#endif
    registerMap[REGMAP_CRC_ADDRESS]     =   REGMAP_CRC_DEFAULT;
}



//****************************************************************************
//
// Helper functions
//
//****************************************************************************


//*****************************************************************************
//
//! Takes a 16-bit word and returns the most-significant byte.
//!
//! \fn uint8_t upperByte(uint16_t uint16_Word)
//!
//! \param temp_word is the original 16-bit word.
//!
//! \return 8-bit most-significant byte.
//
//*****************************************************************************
uint8_t upperByte(uint16_t uint16_Word)
{
    uint8_t msByte;
    msByte = (uint8_t) ((uint16_Word >> 8) & 0x00FF);

    return msByte;
}



//*****************************************************************************
//
//! Takes a 16-bit word and returns the least-significant byte.
//!
//! \fn uint8_t lowerByte(uint16_t uint16_Word)
//!
//! \param temp_word is the original 16-bit word.
//!
//! \return 8-bit least-significant byte.
//
//*****************************************************************************
uint8_t lowerByte(uint16_t uint16_Word)
{
    uint8_t lsByte;
    lsByte = (uint8_t) (uint16_Word & 0x00FF);

    return lsByte;
}



//*****************************************************************************
//
//! Takes two 8-bit words and returns a concatenated 16-bit word.
//!
//! \fn uint16_t combineBytes(uint8_t upperByte, uint8_t lowerByte)
//!
//! \param upperByte is the 8-bit value that will become the MSB of the 16-bit word.
//! \param lowerByte is the 8-bit value that will become the LSB of the 16-bit word.
//!
//! \return concatenated 16-bit word.
//
//*****************************************************************************
uint16_t combineBytes(uint8_t upperByte, uint8_t lowerByte)
{
    uint16_t combinedValue;
    combinedValue = ((uint16_t) upperByte << 8) | ((uint16_t) lowerByte);

    return combinedValue;
}



//*****************************************************************************
//
//! Combines ADC data bytes into a single signed 32-bit word.
//!
//! \fn int32_t combineDataBytes(const uint8_t dataBytes[])
//!
//! \param dataBytes is a pointer to uint8_t[] where the first element is the MSB.
//!
//! \return Returns the signed-extend 32-bit result.
//
//*****************************************************************************
int32_t signExtend(const uint8_t dataBytes[])
{

#ifdef WORD_LENGTH_24BIT

    int32_t upperByte   = ((int32_t) dataBytes[0] << 24);
    int32_t middleByte  = ((int32_t) dataBytes[1] << 16);
    int32_t lowerByte   = ((int32_t) dataBytes[2] << 8);

    return (((int32_t) (upperByte | middleByte | lowerByte)) >> 8);     // Right-shift of signed data maintains signed bit

#elif defined WORD_LENGTH_32BIT_SIGN_EXTEND

    int32_t signByte    = ((int32_t) dataBytes[0] << 24);
    int32_t upperByte   = ((int32_t) dataBytes[1] << 16);
    int32_t middleByte  = ((int32_t) dataBytes[2] << 8);
    int32_t lowerByte   = ((int32_t) dataBytes[3] << 0);

    return (signByte | upperByte | middleByte | lowerByte);

#elif defined WORD_LENGTH_32BIT_ZERO_PADDED

    int32_t upperByte   = ((int32_t) dataBytes[0] << 24);
    int32_t middleByte  = ((int32_t) dataBytes[1] << 16);
    int32_t lowerByte   = ((int32_t) dataBytes[2] << 8);

    return (((int32_t) (upperByte | middleByte | lowerByte)) >> 8);     // Right-shift of signed data maintains signed bit

#elif defined WORD_LENGTH_16BIT_TRUNCATED

    int32_t upperByte   = ((int32_t) dataBytes[0] << 24);
    int32_t lowerByte   = ((int32_t) dataBytes[1] << 16);

    return (((int32_t) (upperByte | lowerByte)) >> 16);                 // Right-shift of signed data maintains signed bit

#endif
}



//****************************************************************************
//
// Internal functions
//
//****************************************************************************


//*****************************************************************************
//
//! Builds SPI TX data arrays according to number of opcodes provided and
//! currently programmed device word length.
//!
//! \fn uint8_t buildSPIarray(const uint16_t opcodeArray[], uint8_t numberOpcodes, uint8_t byteArray[])
//!
//! \param opcodeArray[] pointer to an array of 16-bit opcodes to use in the SPI command.
//! \param numberOpcodes the number of opcodes provided in opcodeArray[].
//! \param byteArray[] pointer to an array of 8-bit SPI bytes to send to the device.
//!
//! NOTE: The calling function must ensure it reserves sufficient memory for byteArray[]!
//!
//! \return number of bytes added to byteArray[].
//
//*****************************************************************************
uint8_t buildSPIarray(const uint16_t opcodeArray[], uint8_t numberOpcodes, uint8_t byteArray[])
{
    /*
     * Frame size = opcode word(s) + optional CRC word
     * Number of bytes per word = 2, 3, or 4
     * Total bytes = bytes per word * number of words
     */
    uint8_t numberWords     = numberOpcodes + (SPI_CRC_ENABLED ? 1 : 0);
    uint8_t bytesPerWord    = 2;//getWordByteLength();
    uint8_t numberOfBytes   = numberWords * bytesPerWord;

    int i;
    for (i = 0; i < numberOpcodes; i++)
    {
        // NOTE: Be careful not to accidentally overflow the array here.
        // The array and opcodes are defined in the calling function, so
        // we are trusting that no mistakes were made in the calling function!
        byteArray[(i*bytesPerWord) + 0] = upperByte(opcodeArray[i]);
        byteArray[(i*bytesPerWord) + 1] = lowerByte(opcodeArray[i]);
    }

#ifdef ENABLE_CRC_IN
    // Calculate CRC and put it into TX array
    uint16_t crcWord = calculateCRC(&byteArray[0], numberOfBytes, 0xFFFF);
    byteArray[(i*bytesPerWord) + 0] = upperByte(crcWord);
    byteArray[(i*bytesPerWord) + 1] = lowerByte(crcWord);
#endif

    return numberOfBytes;
}



//*****************************************************************************
//
//! Modifies MODE register data to maintain device operation according to
//! preselected mode(s) (RX_CRC_EN, WLENGTH, etc.).
//!
//! \fn uint16_t enforce_selected_device_mode(uint16_t data)
//!
//! \param data uint16_t register data.
//!
//! \return uint16_t modified register data.
//
//*****************************************************************************
uint16_t enforce_selected_device_modes(uint16_t data)
{


    ///////////////////////////////////////////////////////////////////////////
    // Enforce RX_CRC_EN setting

#ifdef ENABLE_CRC_IN
    // When writing to the MODE register, ensure RX_CRC_EN bit is ALWAYS set
    data |= MODE_RX_CRC_EN_ENABLED;
#else
    // When writing to the MODE register, ensure RX_CRC_EN bit is NEVER set
    data &= ~MODE_RX_CRC_EN_ENABLED;
#endif // ENABLE_CRC_IN


    ///////////////////////////////////////////////////////////////////////////
    // Enforce WLENGH setting

#ifdef WORD_LENGTH_24BIT
    // When writing to the MODE register, ensure WLENGTH bits are ALWAYS set to 01b
    data = (data & ~MODE_WLENGTH_MASK) | MODE_WLENGTH_24BIT;
#elif defined WORD_LENGTH_32BIT_SIGN_EXTEND
    // When writing to the MODE register, ensure WLENGH bits are ALWAYS set to 11b
    data = (data & ~MODE_WLENGTH_MASK) | MODE_WLENGTH_32BIT_MSB_SIGN_EXT;
#elif defined WORD_LENGTH_32BIT_ZERO_PADDED
    // When writing to the MODE register, ensure WLENGH bits are ALWAYS set to 10b
    data = (data & ~MODE_WLENGTH_MASK) | MODE_WLENGTH_32BIT_LSB_ZEROES;
#elif defined WORD_LENGTH_16BIT_TRUNCATED
    // When writing to the MODE register, ensure WLENGH bits are ALWAYS set to 00b
    data = (data & ~MODE_WLENGTH_MASK) | MODE_WLENGTH_16BIT;
#endif


    ///////////////////////////////////////////////////////////////////////////
    // Enforce DRDY_FMT setting

#ifdef DRDY_FMT_PULSE
    // When writing to the MODE register, ensure DRDY_FMT bit is ALWAYS set
    data = (data & ~MODE_DRDY_FMT_MASK) | MODE_DRDY_FMT_NEG_PULSE_FIXED_WIDTH;
#else
    // When writing to the MODE register, ensure DRDY_FMT bit is NEVER set
    data = (data & ~MODE_DRDY_FMT_MASK) | MODE_DRDY_FMT_LOGIC_LOW;
#endif


    ///////////////////////////////////////////////////////////////////////////
    // Enforce CRC_TYPE setting

#ifdef CRC_CCITT
    // When writing to the MODE register, ensure CRC_TYPE bit is NEVER set
    data = (data & ~STATUS_CRC_TYPE_MASK) | STATUS_CRC_TYPE_16BIT_CCITT;
#elif defined CRC_ANSI
    // When writing to the MODE register, ensure CRC_TYPE bit is ALWAYS set
    data = (data & ~STATUS_CRC_TYPE_MASK) | STATUS_CRC_TYPE_16BIT_ANSI;
#endif

    // Return modified register data
    return data;
}



//*****************************************************************************
//
//! Returns the ADS131M0x configured word length used for SPI communication.
//!
//! \fn uint8_t getWordByteLength(void)
//!
//! NOTE: It is important that the MODE register value stored in registerMap[]
//! remains in sync with the device. If these values get out of sync then SPI
//! communication may fail!
//!
//! \return SPI word byte length (2, 3, or 4)
//
//*****************************************************************************
uint8_t getWordByteLength(void)
{
    return wlength_byte_values[WLENGTH];
}
