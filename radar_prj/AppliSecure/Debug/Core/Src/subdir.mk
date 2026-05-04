################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/ads131m0x.c \
../Core/Src/diskio.c \
../Core/Src/ff.c \
../Core/Src/fft.c \
../Core/Src/main.c \
../Core/Src/radar.c \
../Core/Src/radar_features.c \
../Core/Src/sd_logger.c \
../Core/Src/secure_nsc.c \
../Core/Src/stm32n6xx_hal_msp.c \
../Core/Src/stm32n6xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32n6xx_s.c 

OBJS += \
./Core/Src/ads131m0x.o \
./Core/Src/diskio.o \
./Core/Src/ff.o \
./Core/Src/fft.o \
./Core/Src/main.o \
./Core/Src/radar.o \
./Core/Src/radar_features.o \
./Core/Src/sd_logger.o \
./Core/Src/secure_nsc.o \
./Core/Src/stm32n6xx_hal_msp.o \
./Core/Src/stm32n6xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32n6xx_s.o 

C_DEPS += \
./Core/Src/ads131m0x.d \
./Core/Src/diskio.d \
./Core/Src/ff.d \
./Core/Src/fft.d \
./Core/Src/main.d \
./Core/Src/radar.d \
./Core/Src/radar_features.d \
./Core/Src/sd_logger.d \
./Core/Src/secure_nsc.d \
./Core/Src/stm32n6xx_hal_msp.d \
./Core/Src/stm32n6xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32n6xx_s.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -c -I../Core/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/ads131m0x.cyclo ./Core/Src/ads131m0x.d ./Core/Src/ads131m0x.o ./Core/Src/ads131m0x.su ./Core/Src/diskio.cyclo ./Core/Src/diskio.d ./Core/Src/diskio.o ./Core/Src/diskio.su ./Core/Src/ff.cyclo ./Core/Src/ff.d ./Core/Src/ff.o ./Core/Src/ff.su ./Core/Src/fft.cyclo ./Core/Src/fft.d ./Core/Src/fft.o ./Core/Src/fft.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/radar.cyclo ./Core/Src/radar.d ./Core/Src/radar.o ./Core/Src/radar.su ./Core/Src/radar_features.cyclo ./Core/Src/radar_features.d ./Core/Src/radar_features.o ./Core/Src/radar_features.su ./Core/Src/sd_logger.cyclo ./Core/Src/sd_logger.d ./Core/Src/sd_logger.o ./Core/Src/sd_logger.su ./Core/Src/secure_nsc.cyclo ./Core/Src/secure_nsc.d ./Core/Src/secure_nsc.o ./Core/Src/secure_nsc.su ./Core/Src/stm32n6xx_hal_msp.cyclo ./Core/Src/stm32n6xx_hal_msp.d ./Core/Src/stm32n6xx_hal_msp.o ./Core/Src/stm32n6xx_hal_msp.su ./Core/Src/stm32n6xx_it.cyclo ./Core/Src/stm32n6xx_it.d ./Core/Src/stm32n6xx_it.o ./Core/Src/stm32n6xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32n6xx_s.cyclo ./Core/Src/system_stm32n6xx_s.d ./Core/Src/system_stm32n6xx_s.o ./Core/Src/system_stm32n6xx_s.su

.PHONY: clean-Core-2f-Src

