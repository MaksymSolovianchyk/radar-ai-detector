/*
 * radar.h
 *
 *  Created on: Jul 22, 2025
 *      Author: Emma
 */

#ifndef INC_RADAR_H_
#define INC_RADAR_H_

//Types
typedef struct {
    uint8_t tx[18];    // 18-byte transmit buffer
    uint8_t rx[18];    // 18-byte receive buffer
} ADC;

extern ADC ADC_Buffer;
extern volatile bool adcready;
extern volatile bool adc_data_ready;

//Functions
void ADS_Init(void);
void DrdyPulseCallback(void);
void DrdyExti(void);
void BuffMemCopy(void);
void UartStartSend(void);
void UartStartReceive(void);
void UartRxcallback(void);


#endif /* INC_RADAR_H_ */
