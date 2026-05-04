## Connection for Radar sensor with STM32N6570-DK via STMod+
[STM32N6570-DK Datasheet](https://www.st.com/resource/en/user_manual/um3300-discovery-kit-with-stm32n657x0-mcu-stmicroelectronics.pdf)

| Radar PCB Pin | STM32 Pin | Signal | Description |
|---------------|-----------|--------|-------------|
| VIN | 6 | +5V | Power supply |
| GND | 5 | GND | Ground reference |
| SPI2_CLK | 4 | SPI5_SCK (PE15) | SPI clock |
| SPI2_MISO | 3 | SPI5_MISO (PH8)| Data from ADC to STM32 |
| SPI2_MOSI | 2 | SPI5_MOSI (PG2)| Data from STM32 to ADC |
| ADC_SPI_CS1 | 1 | SPI5_CS (PA3 -> Gpio Output)| Chip select |
| ADC_DRDY | 11 | INT (PC11 -> GPIO_EXTI11) | Data ready interrupt from ADC → MCU (best practice for sampling) |
| ADC_RESET | 12 | RESET (PB3 -> Gpio Output) | Lets MCU reset/sync the ADC. (Using PB3 is convenient; a normal GPIO also works.) |
| EN_RADAR| 13 | GPIO (PC9 -> Gpio Output) | Switch off radar in pulse mode |

### To enable SPI5 on STMod+, follow this physical configuration
![STMod+ Connector Mode](/docs/img/stmod_mode.png)

![STMod+ Connector Pinout](/docs/img/stmod_connector_pinout.png)
![Radar Sensor Pinout](/docs/img/radar_pinout.png)
