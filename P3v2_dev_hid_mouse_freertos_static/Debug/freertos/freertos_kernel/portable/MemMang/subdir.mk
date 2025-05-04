################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../freertos/freertos_kernel/portable/MemMang/heap_4.c 

C_DEPS += \
./freertos/freertos_kernel/portable/MemMang/heap_4.d 

OBJS += \
./freertos/freertos_kernel/portable/MemMang/heap_4.o 


# Each subdirectory must supply rules for building sources it contributes
freertos/freertos_kernel/portable/MemMang/%.o: ../freertos/freertos_kernel/portable/MemMang/%.c freertos/freertos_kernel/portable/MemMang/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MK66FN2M0VMD18 -DCPU_MK66FN2M0VMD18_cm4 -D_DEBUG=1 -DUSB_STACK_FREERTOS -DUSB_STACK_FREERTOS_HEAP_SIZE=32768 -DSDK_OS_FREE_RTOS -DFSL_OSA_BM_TASK_ENABLE=0 -DFSL_OSA_BM_TIMER_CONFIG=0 -DSERIAL_PORT_TYPE_UART=1 -DSDK_DEBUGCONSOLE=1 -DMCUXPRESSO_SDK -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\source" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\device\source\khci" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\include" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\device\source\ehci" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\phy" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\freertos\freertos_kernel\include" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\freertos\freertos_kernel\portable\GCC\ARM_CM4F" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\drivers" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\device" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\utilities" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\component\uart" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\component\serial_manager" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\component\lists" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\device\include" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\device\source" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\component\osa" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\CMSIS" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\board" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\device\class" -I"C:\MCUXpresso\K66_Workspace\P3v2_dev_hid_mouse_freertos_static\usb\device\class\hid" -O0 -fno-common -g3 -gdwarf-4 -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-freertos-2f-freertos_kernel-2f-portable-2f-MemMang

clean-freertos-2f-freertos_kernel-2f-portable-2f-MemMang:
	-$(RM) ./freertos/freertos_kernel/portable/MemMang/heap_4.d ./freertos/freertos_kernel/portable/MemMang/heap_4.o

.PHONY: clean-freertos-2f-freertos_kernel-2f-portable-2f-MemMang

