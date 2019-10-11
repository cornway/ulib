
#include "stm32f7xx_it.h"

#include "int/input_int.h"
#include <usbh_def.h>
#include <usbh_conf.h>
#include <usbh_core.h>
#include <usbh_hid.h>
#include <misc_utils.h>
#include <input_main.h>
#include <nvic.h>

#if defined(BSP_DRIVER)

static void USBH_UserProcess(USBH_HandleTypeDef * phost, uint8_t id);

void *g_usb_data;
uint32_t g_usb_data_size;
int8_t g_usb_data_ready = 0;

static USBH_HandleTypeDef hUSBHost;

static irqmask_t usb_irq;
static gamepad_drv_t joypad_drv = {NULL};
static uint8_t joypad_ready = 0;
static uint32_t jpad_last_tsf;

int joypad_read (int8_t *pads)
{
    uint8_t data[64];
    irqmask_t irq = usb_irq;

    if (!d_rlimit_wrap(&jpad_last_tsf, 1000 / 25)) {
        return 0;
    }

    if (!g_usb_data_ready) {
        return 0;
    }
    g_usb_data_ready = 0;

    assert(sizeof(data) > g_usb_data_size);

    irq_save(&irq);
    d_memcpy(&data, g_usb_data, g_usb_data_size);
    irq_restore(irq);

    if (joypad_drv.read) {
        return joypad_drv.read(&joypad_drv, pads, &data[0]);
    }
    return 0;
}

void joypad_bsp_deinit (void)
{
    USBH_Stop(&hUSBHost);
    USBH_DeInit(&hUSBHost);
    USBH_LL_DeInit(&hUSBHost);
    if (joypad_drv.deinit) {
        joypad_drv.deinit(&joypad_drv);
    }
    d_memzero(&joypad_drv, sizeof(joypad_drv));
    g_usb_data = NULL;
    g_usb_data_size = 0;
}

void joypad_bsp_init (void)
{
    irqmask_t temp;
    uint32_t timeout = 2000;

    irq_bmap(&temp);
    USBH_Init(&hUSBHost, USBH_UserProcess, 0);
    USBH_RegisterClass(&hUSBHost, USBH_HID_CLASS);
    USBH_Start(&hUSBHost);
    irq_bmap(&usb_irq);
    usb_irq = usb_irq & (~temp);
    timeout += d_time();
    joypad_ready = 0;
    while (timeout > d_time() && !joypad_ready) {
        joypad_tickle();
        d_sleep(10);
    }
}

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{

}

void joypad_tickle (void)
{
    USBH_Process(&hUSBHost);
}

static void USBH_UserProcess(USBH_HandleTypeDef * phost, uint8_t id)
{

}

USBH_StatusTypeDef USBH_HID_GamepadInit(USBH_HandleTypeDef *phost)
{
    HID_HandleTypeDef *HID_Handle =  (HID_HandleTypeDef *) phost->pActiveClass->pData;

    if (joypad_ready) {
        return USBH_FAIL;
    }
    if (phost->device.DevDesc.idProduct == 0x1 &&
        phost->device.DevDesc.idVendor == 0x810) {

        joypad_attach_gp_50(&joypad_drv);
    } else {
        joypad_attach_def(&joypad_drv);
    }

    if (joypad_drv.init) {
        g_usb_data = joypad_drv.init(&joypad_drv, &g_usb_data_size);
    }

    if (NULL == g_usb_data) {
        return USBH_FAIL;
    }
    HID_Handle->length = g_usb_data_size;
    HID_Handle->pData = (uint8_t *)g_usb_data;

    fifo_init(&HID_Handle->fifo, phost->device.Data, HID_Handle->length);
    joypad_ready = 1;
    return USBH_OK;
}

extern HCD_HandleTypeDef hhcd;

#ifdef USE_USB_FS
void OTG_FS_IRQHandler(void)
#else
void OTG_HS_IRQHandler(void)
#endif
{
  HAL_HCD_IRQHandler(&hhcd);
}

#endif /*BSP_DRIVER*/
