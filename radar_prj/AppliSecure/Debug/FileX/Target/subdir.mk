################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FileX/Target/fx_stm32_sd_driver_glue.c 

OBJS += \
./FileX/Target/fx_stm32_sd_driver_glue.o 

C_DEPS += \
./FileX/Target/fx_stm32_sd_driver_glue.d 


# Each subdirectory must supply rules for building sources it contributes
FileX/Target/%.o FileX/Target/%.su FileX/Target/%.cyclo: ../FileX/Target/%.c FileX/Target/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DFX_INCLUDE_USER_DEFINE_FILE -c -I../FileX/App -I../FileX/Target -I"D:/radar-ai-detector/radar_prj/AppliSecure/FileX/App" -I"D:/radar-ai-detector/radar_prj/AppliSecure/FileX/Target" -I../Core/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Middlewares/ST/filex/common/inc -I../../Middlewares/ST/filex/ports/generic/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FileX-2f-Target

clean-FileX-2f-Target:
	-$(RM) ./FileX/Target/fx_stm32_sd_driver_glue.cyclo ./FileX/Target/fx_stm32_sd_driver_glue.d ./FileX/Target/fx_stm32_sd_driver_glue.o ./FileX/Target/fx_stm32_sd_driver_glue.su

.PHONY: clean-FileX-2f-Target

