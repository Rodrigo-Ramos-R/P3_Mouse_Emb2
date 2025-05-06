/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017,2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
/*${standard_header_anchor}*/
#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"

#include "usb_device_class.h"
#include "usb_device_hid.h"
#include "usb_device_ch9.h"
#include "usb_device_descriptor.h"
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))
#include "usb_hsdcd.h"
#elif (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U))
#include "usb_phydcd.h"
#endif
#include "mouse.h"

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

#include "usb_phy.h"

#include "fsl_debug_console.h"
#include "math.h"
#include "fsl_fxos.h"
#include "pin_mux.h"
#include "peripherals.h"
#include "board.h"
#include "GPIO.h"
#include "NVIC.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define I2C_RELEASE_BUS_COUNT 100U
BaseType_t g_flag_SW2 = pdFALSE;
BaseType_t g_flag_SW3 = pdFALSE;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_InitHardware(void);
void USB_DeviceClockInit(void);
void USB_DeviceIsrEnable(void);
void SensorFXOSinit(void);
void IMU_task(void);
void Callback_SW2(void);
void Callback_SW3(void);

void BOARD_I2C_ReleaseBus(void);
static void Board_UpdatePwm(uint16_t x, uint16_t y);

#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle);
#endif

static usb_status_t USB_DeviceHidMouseAction(void);
static usb_status_t USB_DeviceHidMouseCallback(class_handle_t handle, uint32_t event, void *param);
static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param);
static void USB_DeviceApplicationInit(void);
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))

#elif (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U))
extern void HW_TimerControl(uint8_t enable);
#endif
/*******************************************************************************
 * Variables
 ******************************************************************************/

volatile int16_t xAngle = 0;
volatile int16_t yAngle = 0;

fxos_data_t sensorData   = {0};
fxos_handle_t fxosHandle = {0};
uint8_t dataScale        = 0;
/* FXOS device address */
const uint8_t g_accel_address[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_MouseBuffer[USB_HID_MOUSE_REPORT_LENGTH];
usb_hid_mouse_struct_t g_UsbDeviceHidMouse;

extern usb_device_class_struct_t g_UsbDeviceHidMouseConfig;

/* Set class configurations */
usb_device_class_config_struct_t g_UsbDeviceHidConfig[1] = {{
    USB_DeviceHidMouseCallback, /* HID mouse class callback pointer */
    (class_handle_t)NULL,       /* The HID class handle, This field is set by USB_DeviceClassInit */
    &g_UsbDeviceHidMouseConfig, /* The HID mouse configuration, including class code, subcode, and protocol, class type,
                           transfer type, endpoint address, max packet size, etc.*/
}};

/* Set class configuration list */
usb_device_class_config_list_struct_t g_UsbDeviceHidConfigList = {
    g_UsbDeviceHidConfig, /* Class configurations */
    USB_DeviceCallback,   /* Device callback pointer */
    1U,                   /* Class count */
};

/*******************************************************************************
 * Code
 ******************************************************************************/
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U))
void USBHS_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(g_UsbDeviceHidMouse.deviceHandle);
    /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
    exception return operation might vector to incorrect interrupt */
    __DSB();
}
#endif
#if (defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U))
void USB0_IRQHandler(void)
{
    USB_DeviceKhciIsrFunction(g_UsbDeviceHidMouse.deviceHandle);
    /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
    exception return operation might vector to incorrect interrupt */
    __DSB();
}
#endif
void USB_DeviceClockInit(void)
{
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL,
        BOARD_USB_PHY_TXCAL45DP,
        BOARD_USB_PHY_TXCAL45DM,
    };
#endif
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_UsbPhySrcExt, BOARD_XTAL0_CLK_HZ);
    CLOCK_EnableUsbhs0Clock(kCLOCK_UsbSrcUnused, 0U);
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    SystemCoreClockUpdate();
    CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcIrc48M, 48000000U);
/*
 * If the SOC has USB KHCI dedicated RAM, the RAM memory needs to be clear after
 * the KHCI clock is enabled. When the demo uses USB EHCI IP, the USB KHCI dedicated
 * RAM can not be used and the memory can't be accessed.
 */
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U))
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS) && (FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS > 0U))
    for (int i = 0; i < FSL_FEATURE_USB_KHCI_USB_RAM; i++)
    {
        ((uint8_t *)FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS)[i] = 0x00U;
    }
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS */
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM */
#endif
}
void USB_DeviceIsrEnable(void)
{
    uint8_t irqNumber;
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    uint8_t usbDeviceEhciIrq[] = USBHS_IRQS;
    irqNumber                  = usbDeviceEhciIrq[CONTROLLER_ID - kUSB_ControllerEhci0];
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    uint8_t usbDeviceKhciIrq[] = USB_IRQS;
    irqNumber                  = usbDeviceKhciIrq[CONTROLLER_ID - kUSB_ControllerKhci0];
#endif
    /* Install isr, set priority, and enable IRQ. */
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
    EnableIRQ((IRQn_Type)irqNumber);
}
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle)
{
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    USB_DeviceEhciTaskFunction(deviceHandle);
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    USB_DeviceKhciTaskFunction(deviceHandle);
#endif
}
#endif

/* Update mouse pointer location. Draw a rectangular rotation*/
static usb_status_t USB_DeviceHidMouseAction(void)
{
    if(pdTRUE == g_flag_SW2){
    	g_UsbDeviceHidMouse.buffer[0] = 2U; //Left click?
    	g_flag_SW2 = pdFALSE;
    }

    if(pdTRUE == g_flag_SW3){
    	g_UsbDeviceHidMouse.buffer[0] = 1U; //Rigth click?
    	g_flag_SW3 = pdFALSE;
    }

    IMU_task();  // Actualiza xAngle y yAngle (globales)

    int8_t mouseX = 0;
    int8_t mouseY = 0;

    // Rango de xAngle/yAngle: 0 a 90 -> Convertimos a -10 a 10
    // Puedes ajustar este factor para controlar la sensibilidad
    mouseX = (int8_t)((xAngle - 45) / 4);  // -11 a +11
    mouseY = (int8_t)((yAngle - 45) / 4);  // -11 a +11

    // Limita a valores permitidos para HID (normalmente de -127 a 127, pero aquí usamos algo más seguro)
    if (mouseX > 10){
    	mouseX = 10;
    }
    if (mouseX < -10){
    	mouseX = -10;
    }
    if (mouseY > 10){
    	mouseY = 10;
    }
    if (mouseY < -10){
    	mouseY = -10;
    }

    g_UsbDeviceHidMouse.buffer[1] = mouseX;     // Movimiento en X
    g_UsbDeviceHidMouse.buffer[2] = mouseY;     // Movimiento en Y

    return USB_DeviceHidSend(g_UsbDeviceHidMouse.hidHandle,
                             USB_HID_MOUSE_ENDPOINT_IN,
                             g_UsbDeviceHidMouse.buffer,
                             USB_HID_MOUSE_REPORT_LENGTH);
}



static void i2c_release_bus_delay(void)
{
    uint32_t i = 0;
    for (i = 0; i < I2C_RELEASE_BUS_COUNT; i++)
    {
        __NOP();
    }
}

void BOARD_I2C_ReleaseBus(void)
{
    uint8_t i = 0;

    BOARD_Init_I2C_GPIO_pins();

    /* Drive SDA low first to simulate a start */
    GPIO_PinWrite(BOARD_ACCEL_I2C_SDA_GPIO, BOARD_ACCEL_I2C_SDA_PIN, 0U);
    i2c_release_bus_delay();

    /* Send 9 pulses on SCL and keep SDA high */
    for (i = 0; i < 9; i++)
    {
        GPIO_PinWrite(BOARD_ACCEL_I2C_SCL_GPIO, BOARD_ACCEL_I2C_SCL_PIN, 0U);
        i2c_release_bus_delay();

        GPIO_PinWrite(BOARD_ACCEL_I2C_SDA_GPIO, BOARD_ACCEL_I2C_SDA_PIN, 1U);
        i2c_release_bus_delay();

        GPIO_PinWrite(BOARD_ACCEL_I2C_SCL_GPIO, BOARD_ACCEL_I2C_SCL_PIN, 1U);
        i2c_release_bus_delay();
        i2c_release_bus_delay();
    }

    /* Send stop */
    GPIO_PinWrite(BOARD_ACCEL_I2C_SCL_GPIO, BOARD_ACCEL_I2C_SCL_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(BOARD_ACCEL_I2C_SDA_GPIO, BOARD_ACCEL_I2C_SDA_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(BOARD_ACCEL_I2C_SCL_GPIO, BOARD_ACCEL_I2C_SCL_PIN, 1U);
    i2c_release_bus_delay();

    GPIO_PinWrite(BOARD_ACCEL_I2C_SDA_GPIO, BOARD_ACCEL_I2C_SDA_PIN, 1U);
    i2c_release_bus_delay();
}
/* Update the duty cycle of an active pwm signal */
static void Board_UpdatePwm(uint16_t x, uint16_t y)
{
    /* Start PWM mode with updated duty cycle */
    FTM_UpdatePwmDutycycle(BOARD_TIMER_PERIPHERAL, kFTM_Chnl_5, kFTM_EdgeAlignedPwm, x);
    FTM_UpdatePwmDutycycle(BOARD_TIMER_PERIPHERAL, kFTM_Chnl_1, kFTM_EdgeAlignedPwm, y);
    /* Software trigger to update registers */
    FTM_SetSoftwareTrigger(BOARD_TIMER_PERIPHERAL, true);
}

/* The hid class callback */
static usb_status_t USB_DeviceHidMouseCallback(class_handle_t handle, uint32_t event, void *param)
{
    usb_status_t error                                     = kStatus_USB_InvalidRequest;
    usb_device_endpoint_callback_message_struct_t *message = (usb_device_endpoint_callback_message_struct_t *)param;

    switch (event)
    {
        case kUSB_DeviceHidEventSendResponse:
            /* Resport sent */
            if (g_UsbDeviceHidMouse.attach)
            {
                /* endpoint callback length is USB_CANCELLED_TRANSFER_LENGTH (0xFFFFFFFFU) when transfer is canceled */
                if ((NULL != message) && (message->length == USB_CANCELLED_TRANSFER_LENGTH))
                {
                    return error;
                }
                error = USB_DeviceHidMouseAction();
            }
            break;
        case kUSB_DeviceHidEventGetReport:
        case kUSB_DeviceHidEventSetReport:
        case kUSB_DeviceHidEventRequestReportBuffer:
            break;
        case kUSB_DeviceHidEventGetIdle:
        case kUSB_DeviceHidEventGetProtocol:
        case kUSB_DeviceHidEventSetIdle:
        case kUSB_DeviceHidEventSetProtocol:
            error = kStatus_USB_Success;
            break;
        default:
            break;
    }

    return error;
}

/* The device callback */
static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint16_t *temp16   = (uint16_t *)param;
    uint8_t *temp8     = (uint8_t *)param;

    switch (event)
    {
        case kUSB_DeviceEventBusReset:
        {
            /* USB bus reset signal detected */
            g_UsbDeviceHidMouse.attach               = 0U;
            g_UsbDeviceHidMouse.currentConfiguration = 0U;
            error                                    = kStatus_USB_Success;
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)) || \
    (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
            /* Get USB speed to configure the device, including max packet size and interval of the endpoints. */
            if (kStatus_USB_Success == USB_DeviceClassGetSpeed(CONTROLLER_ID, &g_UsbDeviceHidMouse.speed))
            {
                USB_DeviceSetSpeed(handle, g_UsbDeviceHidMouse.speed);
            }
#endif
        }
        break;
#if (defined(USB_DEVICE_CONFIG_DETACH_ENABLE) && (USB_DEVICE_CONFIG_DETACH_ENABLE > 0U))
        case kUSB_DeviceEventAttach:
        {
            g_UsbDeviceHidMouse.connectStateChanged = 1U;
            g_UsbDeviceHidMouse.connectState        = 1U;
            error                                   = kStatus_USB_Success;
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (((defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))) ||  \
     (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U)))
            g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionInit;
#else

#if (defined(USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS > 0U))
#else
            /*Add one delay here to make the DP pull down long enough to allow host to detect the previous
             * disconnection.*/
            SDK_DelayAtLeastUs(5000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
            USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
#endif
#endif
        }
        break;
        case kUSB_DeviceEventDetach:
        {
            g_UsbDeviceHidMouse.connectStateChanged = 1U;
            g_UsbDeviceHidMouse.connectState        = 0U;
            g_UsbDeviceHidMouse.attach              = 0U;
            error                                   = kStatus_USB_Success;
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (((defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))) ||  \
     (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U)))
            g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionInit;
#else

#if (defined(USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS > 0U))
#else
            USB_DeviceStop(g_UsbDeviceHidMouse.deviceHandle);
#endif

#endif
        }
        break;
#endif
        case kUSB_DeviceEventSetConfiguration:
            if (0U == (*temp8))
            {
                g_UsbDeviceHidMouse.attach               = 0;
                g_UsbDeviceHidMouse.currentConfiguration = 0U;
                error                                    = kStatus_USB_Success;
            }
            else if (USB_HID_MOUSE_CONFIGURE_INDEX == (*temp8))
            {
                /* Set device configuration request */
                g_UsbDeviceHidMouse.attach               = 1U;
                g_UsbDeviceHidMouse.currentConfiguration = *temp8;
                error                                    = USB_DeviceHidMouseAction();
            }
            else
            {
                /* no action required, the default return value is kStatus_USB_InvalidRequest. */
            }
            break;
        case kUSB_DeviceEventSetInterface:
            if (g_UsbDeviceHidMouse.attach)
            {
                /* Set device interface request */
                uint8_t interface        = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                uint8_t alternateSetting = (uint8_t)(*temp16 & 0x00FFU);
                if (interface < USB_HID_MOUSE_INTERFACE_COUNT)
                {
                    if (alternateSetting < USB_HID_MOUSE_INTERFACE_ALTERNATE_COUNT)
                    {
                        g_UsbDeviceHidMouse.currentInterfaceAlternateSetting[interface] = alternateSetting;
                        if (alternateSetting == 0U)
                        {
                            error = USB_DeviceHidMouseAction();
                        }
                    }
                }
            }
            break;
        case kUSB_DeviceEventGetConfiguration:
            if (param)
            {
                /* Get current configuration request */
                *temp8 = g_UsbDeviceHidMouse.currentConfiguration;
                error  = kStatus_USB_Success;
            }
            break;
        case kUSB_DeviceEventGetInterface:
            if (param)
            {
                /* Get current alternate setting of the interface request */
                uint8_t interface = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                if (interface < USB_HID_MOUSE_INTERFACE_COUNT)
                {
                    *temp16 = (*temp16 & 0xFF00U) | g_UsbDeviceHidMouse.currentInterfaceAlternateSetting[interface];
                    error   = kStatus_USB_Success;
                }
            }
            break;
        case kUSB_DeviceEventGetDeviceDescriptor:
            if (param)
            {
                /* Get device descriptor request */
                error = USB_DeviceGetDeviceDescriptor(handle, (usb_device_get_device_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetConfigurationDescriptor:
            if (param)
            {
                /* Get device configuration descriptor request */
                error = USB_DeviceGetConfigurationDescriptor(handle,
                                                             (usb_device_get_configuration_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetStringDescriptor:
            if (param)
            {
                /* Get device string descriptor request */
                error = USB_DeviceGetStringDescriptor(handle, (usb_device_get_string_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetHidDescriptor:
            if (param)
            {
                /* Get hid descriptor request */
                error = USB_DeviceGetHidDescriptor(handle, (usb_device_get_hid_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetHidReportDescriptor:
            if (param)
            {
                /* Get hid report descriptor request */
                error =
                    USB_DeviceGetHidReportDescriptor(handle, (usb_device_get_hid_report_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetHidPhysicalDescriptor:
            if (param)
            {
                /* Get hid physical descriptor request */
                error = USB_DeviceGetHidPhysicalDescriptor(handle,
                                                           (usb_device_get_hid_physical_descriptor_struct_t *)param);
            }
            break;
#if (defined(USB_DEVICE_CONFIG_CV_TEST) && (USB_DEVICE_CONFIG_CV_TEST > 0U))
        case kUSB_DeviceEventGetDeviceQualifierDescriptor:
            if (param)
            {
                /* Get device descriptor request */
                error = USB_DeviceGetDeviceQualifierDescriptor(
                    handle, (usb_device_get_device_qualifier_descriptor_struct_t *)param);
            }
            break;
#endif
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (((defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))) ||  \
     (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U)))
        case kUSB_DeviceEventDcdDetectionfinished:
            /*temp pointer point to detection result*/
            if (param)
            {
                error = kStatus_USB_Success;
                if (kUSB_DcdSDP == *temp8)
                {
                    g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionSDP;
                }
                else if (kUSB_DcdCDP == *temp8)
                {
                    g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionCDP;
                }
                else if (kUSB_DcdDCP == *temp8)
                {
                    g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionDCP;
                }
                else if (kUSB_DcdTimeOut == *temp8)
                {
                    g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionTimeOut;
                }
                else if (kUSB_DcdError == *temp8)
                {
                    g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionError;
                }
                else
                {
                    error = kStatus_USB_InvalidRequest;
                }
            }
            break;
#endif
        default:
            /* no action required, the default return value is kStatus_USB_InvalidRequest. */
            break;
    }

    return error;
}

static void USB_DeviceApplicationInit(void)
{
    USB_DeviceClockInit();
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
    SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

    /* Set HID mouse to default state */
    g_UsbDeviceHidMouse.speed        = USB_SPEED_FULL;
    g_UsbDeviceHidMouse.attach       = 0U;
    g_UsbDeviceHidMouse.hidHandle    = (class_handle_t)NULL;
    g_UsbDeviceHidMouse.deviceHandle = NULL;
    g_UsbDeviceHidMouse.buffer       = s_MouseBuffer;

    /* Initialize the usb stack and class drivers */
    if (kStatus_USB_Success !=
        USB_DeviceClassInit(CONTROLLER_ID, &g_UsbDeviceHidConfigList, &g_UsbDeviceHidMouse.deviceHandle))
    {
        usb_echo("USB device mouse failed\r\n");
        return;
    }
    else
    {
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (((defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))) ||  \
     (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U)))
        usb_echo("USB device DCD + HID mouse demo\r\n");
#else
        usb_echo("USB device HID mouse demo\r\n");
#endif
        /* Get the HID mouse class handle */
        g_UsbDeviceHidMouse.hidHandle = g_UsbDeviceHidConfigList.config->classHandle;
    }
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (((defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))) ||  \
     (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U)))
    g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionInit;
#endif

    USB_DeviceIsrEnable();

#if (defined(USB_DEVICE_CONFIG_DETACH_ENABLE) && (USB_DEVICE_CONFIG_DETACH_ENABLE > 0U))
    /*USB_DeviceRun could not be called here to avoid DP/DM confliction between DCD function and USB function in case
      DCD is enabled. Instead, USB_DeviceRun should be called after the DCD is finished immediately*/
#if (defined(USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS > 0U))
    /* Start USB device HID mouse */
    /*Add one delay here to make the DP pull down long enough to allow host to detect the previous disconnection.*/
    SDK_DelayAtLeastUs(5000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
#endif
#else
    /* Start USB device HID mouse */
    /*Add one delay here to make the DP pull down long enough to allow host to detect the previous disconnection.*/
    SDK_DelayAtLeastUs(5000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
#endif
}

#if defined(USB_DEVICE_CONFIG_USE_TASK) && (USB_DEVICE_CONFIG_USE_TASK > 0)
void USB_DeviceTask(void *handle)
{
    while (1U)
    {
        USB_DeviceTaskFn(handle);
    }
}
#endif

void APP_task(void *handle)
{
    USB_DeviceApplicationInit();

#if USB_DEVICE_CONFIG_USE_TASK
    if (g_UsbDeviceHidMouse.deviceHandle)
    {
        if (xTaskCreate(USB_DeviceTask,                       /* pointer to the task */
                        "usb device task",                    /* task name for kernel awareness debugging */
                        5000L / sizeof(portSTACK_TYPE),       /* task stack size */
                        g_UsbDeviceHidMouse.deviceHandle,     /* optional task startup argument */
                        5U,                                   /* initial priority */
                        &g_UsbDeviceHidMouse.deviceTaskHandle /* optional task handle to create */
                        ) != pdPASS)
        {
            usb_echo("usb device task create failed!\r\n");
            return;
        }
    }
#endif

    while (1U)
    {
#if (defined(USB_DEVICE_CONFIG_DETACH_ENABLE) && (USB_DEVICE_CONFIG_DETACH_ENABLE > 0U))

        if (g_UsbDeviceHidMouse.connectStateChanged)
        {
            g_UsbDeviceHidMouse.connectStateChanged = 0;
            if (g_UsbDeviceHidMouse.connectState)
            {
                /*user need call USB_DeviceRun here to usb function run if dcd function is disabled*/
                /*USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);*/
                usb_echo("USB device attached.\r\n");
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))

#elif (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U))
                HW_TimerControl(1U);
#endif
            }
            else
            {
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U))
                /*USB_DeviceStop should be called here to avoid DP/DM confliction between DCD function and USB function
                 * in case next time DCD dection. */
                USB_DeviceStop(g_UsbDeviceHidMouse.deviceHandle);
#endif
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))

#elif (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U))
                HW_TimerControl(0U);
#endif
                usb_echo("USB device detached.\r\n");
            }
        }
#if (defined(USB_DEVICE_CONFIG_CHARGER_DETECT) && (USB_DEVICE_CONFIG_CHARGER_DETECT > 0U)) && \
    (((defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U))) ||  \
     (defined(FSL_FEATURE_SOC_USB_ANALOG_COUNT) && (FSL_FEATURE_SOC_USB_ANALOG_COUNT > 0U)))
        if ((kUSB_DeviceDCDDectionInit != g_UsbDeviceHidMouse.dcdDectionStatus) &&
            (kUSB_DeviceDCDDectionFinished != g_UsbDeviceHidMouse.dcdDectionStatus))
        {
            switch (g_UsbDeviceHidMouse.dcdDectionStatus)
            {
                case kUSB_DeviceDCDDectionSDP:
                {
                    usb_echo("SDP(standard downstream port) is detected.\r\n");
                    /* Start USB device HID mouse */
                    USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
                }
                break;
                case kUSB_DeviceDCDDectionDCP:
                {
                    usb_echo("DCP (dedicated charging port) is detected.\r\n");
#if (defined(FSL_FEATURE_USBPHY_HAS_DCD_ANALOG) && (FSL_FEATURE_USBPHY_HAS_DCD_ANALOG > 0U))
                    /*
                     * This is a work-around to fix the DCP device detach event missing issue.
                     * The device (IP3511HS controller) VBUSDEBOUNCED bit is not updated on time before softeware read
                     * this bit, so when DCP is detached from usb port, softeware can't detect DCP disconnection.
                     */
                    USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
#endif
                }
                break;
                case kUSB_DeviceDCDDectionCDP:
                {
                    usb_echo("CDP(charging downstream port) is detected.\r\n");
                    /* Start USB device HID mouse */
                    USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
                }
                break;
                case kUSB_DeviceDCDDectionTimeOut:
                {
                    usb_echo("Timeout error.\r\n");
                }
                break;
                case kUSB_DeviceDCDDectionError:
                {
                    usb_echo("Detect error.\r\n");
                }
                break;
                default:
                    break;
            }
            g_UsbDeviceHidMouse.dcdDectionStatus = kUSB_DeviceDCDDectionFinished;
        }
#endif
#endif
    }
}

#if defined(__CC_ARM) || (defined(__ARMCC_VERSION)) || defined(__GNUC__)
int main(void)
#else
void main(void)
#endif
{

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_I2C_ReleaseBus();
    BOARD_I2C_ConfigurePins();
    BOARD_InitDebugConsole();
    BOARD_InitPeripherals();

    SensorFXOSinit();

    GPIO_init();

	CLOCK_SetSimSafeDivs();

	/* Callbacks for Switches */
	GPIO_callback_init(GPIO_A, Callback_SW3);
	GPIO_callback_init(GPIO_D, Callback_SW2);

	/* NVIC */
	NVIC_set_basepri_threshold(PRIORITY_10);
	NVIC_enable_interrupt_and_priotity(PORTD_IRQ, PRIORITY_5);
	NVIC_enable_interrupt_and_priotity(PORTA_IRQ, PRIORITY_4);
	NVIC_global_enable_interrupts;


    if (xTaskCreate(APP_task,                                  /* pointer to the task */
                    "app task",                                /* task name for kernel awareness debugging */
                    5000L / sizeof(portSTACK_TYPE),            /* task stack size */
                    &g_UsbDeviceHidMouse,                      /* optional task startup argument */
                    4U,                                        /* initial priority */
                    &g_UsbDeviceHidMouse.applicationTaskHandle /* optional task handle to create */
                    ) != pdPASS)
    {
        usb_echo("app task create failed!\r\n");
#if (defined(__CC_ARM) || (defined(__ARMCC_VERSION)) || defined(__GNUC__))
        return 1U;
#else
        return;
#endif
    }

    vTaskStartScheduler();

#if (defined(__CC_ARM) || (defined(__ARMCC_VERSION)) || defined(__GNUC__))
    return 1U;
#endif
}

void IMU_task(void){

    int16_t xData            = 0;
    int16_t yData            = 0;

	/* Get new accelerometer data. */
	if (FXOS_ReadSensorData(&fxosHandle, &sensorData) != kStatus_Success)
	{
		PRINTF("\r\nSensor device initialize failed!\r\n");
	}

	/* Get the X and Y data from the sensor data structure in 14 bit left format data*/
	xData = (int16_t)((uint16_t)((uint16_t)sensorData.accelXMSB << 8) | (uint16_t)sensorData.accelXLSB) / 4U;
	yData = (int16_t)((uint16_t)((uint16_t)sensorData.accelYMSB << 8) | (uint16_t)sensorData.accelYLSB) / 4U;

	/* Convert raw data to angle (normalize to 0-90 degrees). No negative angles. */
	xAngle = (int16_t)floor((double)xData * (double)dataScale * 90 / 8192);
	if (xAngle < 0)
	{
		xAngle *= -1;
	}
	yAngle = (int16_t)floor((double)yData * (double)dataScale * 90 / 8192);
	if (yAngle < 0)
	{
		yAngle *= -1;
	}

	/* Update the duty cycle of PWM */
	Board_UpdatePwm(xAngle, yAngle);

	/* Print out the angle data. */
	PRINTF("x= %2d y = %2d\r\n", xAngle, yAngle);
}

void SensorFXOSinit(void){

    fxos_config_t config     = {0};
    uint8_t sensorRange      = 0;
    uint8_t i                = 0;
    uint8_t array_addr_size  = 0;
    status_t result          = kStatus_Fail;

    /* Configure the I2C function */
    config.I2C_SendFunc    = BOARD_Accel_I2C_Send;
    config.I2C_ReceiveFunc = BOARD_Accel_I2C_Receive;

    array_addr_size = sizeof(g_accel_address) / sizeof(g_accel_address[0]);
    for (i = 0; i < array_addr_size; i++)
    {
        config.slaveAddress = g_accel_address[i];
        /* Initialize accelerometer sensor */
        result = FXOS_Init(&fxosHandle, &config);
        if (result == kStatus_Success)
        {
            break;
        }
    }

    if (result != kStatus_Success)
    {
        PRINTF("\r\nSensor device initialize failed!\r\n");
    }

    /* Get sensor range */
    if (FXOS_ReadReg(&fxosHandle, XYZ_DATA_CFG_REG, &sensorRange, 1) != kStatus_Success)
    {
        PRINTF("\r\nSensor device initialize failed!\r\n");
    }
    if (sensorRange == 0x00)
    {
        dataScale = 2U;
    }
    else if (sensorRange == 0x01)
    {
        dataScale = 4U;
    }
    else if (sensorRange == 0x10)
    {
        dataScale = 8U;
    }
    else
    {
    }
    /* Start timer */
    FTM_StartTimer(BOARD_TIMER_PERIPHERAL, kFTM_SystemClock);

    /* Print a note to terminal */
    PRINTF("\r\nWelcome to the BUBBLE example\r\n");
    PRINTF("\r\nYou will see the change of angle data and LED brightness when change the angles of board\r\n");
}

void Callback_SW2(void) {
	g_flag_SW2 = pdTRUE;
}


void Callback_SW3(void) {
	g_flag_SW3 = pdTRUE;
}



