################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
D:/radar-ai-detector/radar_prj/Middlewares/ST/filex/common/drivers/fx_stm32_sd_driver.c 

OBJS += \
./Middlewares/ST/SD\ interface/fx_stm32_sd_driver.o 

C_DEPS += \
./Middlewares/ST/SD\ interface/fx_stm32_sd_driver.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/ST/SD\ interface/fx_stm32_sd_driver.o: D:/radar-ai-detector/radar_prj/Middlewares/ST/filex/common/drivers/fx_stm32_sd_driver.c Middlewares/ST/SD\ interface/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DFX_INCLUDE_USER_DEFINE_FILE -c -I../FileX/App -I../FileX/Target -I"D:/radar-ai-detector/radar_prj/AppliSecure/FileX/App" -I"D:/radar-ai-detector/radar_prj/AppliSecure/FileX/Target" -I../Core/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Middlewares/ST/filex/common/inc -I../../Middlewares/ST/filex/ports/generic/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"Middlewares/ST/SD interface/fx_stm32_sd_driver.d" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-ST-2f-SD-20-interface

clean-Middlewares-2f-ST-2f-SD-20-interface:
	-$(RM) ./Middlewares/ST/SD\ interface/fx_stm32_sd_driver.cyclo ./Middlewares/ST/SD\ interface/fx_stm32_sd_driver.d ./Middlewares/ST/SD\ interface/fx_stm32_sd_driver.o ./Middlewares/ST/SD\ interface/fx_stm32_sd_driver.su

.PHONY: clean-Middlewares-2f-ST-2f-SD-20-interface

