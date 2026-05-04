################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FileX/App/app_filex.c 

OBJS += \
./FileX/App/app_filex.o 

C_DEPS += \
./FileX/App/app_filex.d 


# Each subdirectory must supply rules for building sources it contributes
FileX/App/%.o FileX/App/%.su FileX/App/%.cyclo: ../FileX/App/%.c FileX/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DFX_INCLUDE_USER_DEFINE_FILE -c -I../FileX/App -I../FileX/Target -I"D:/radar-ai-detector/radar_prj/AppliSecure/FileX/App" -I"D:/radar-ai-detector/radar_prj/AppliSecure/FileX/Target" -I../Core/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Middlewares/ST/filex/common/inc -I../../Middlewares/ST/filex/ports/generic/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FileX-2f-App

clean-FileX-2f-App:
	-$(RM) ./FileX/App/app_filex.cyclo ./FileX/App/app_filex.d ./FileX/App/app_filex.o ./FileX/App/app_filex.su

.PHONY: clean-FileX-2f-App

