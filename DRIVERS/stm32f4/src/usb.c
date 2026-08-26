/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    usb.c
 * @brief   USB CDC (virtual COM port) transport for STM32F4.
 */

#include "usb.h"

/* -------------------------------------------------------------------------- */
/* Availability                                                               */
/* -------------------------------------------------------------------------- */
/*
 * USB_CDC_AVAILABLE is defined only when the board's CubeMX project actually
 * generated the USB Device middleware. Without it there is no
 * MX_USB_DEVICE_Init() or CDC_Transmit_FS() to link against, so every entry
 * point below compiles to a stub returning DRV_ERR_STATE.
 *
 * This is why the board identity macros exist: the difference between the two
 * boards here is not a register layout that could be handled at runtime, it is
 * whether a whole body of generated code is present in the build at all.
 *
 * To enable on the DISC1: generate the middleware per the steps in usb.h, then
 * add USB_CDC_AVAILABLE to the board's compile definitions — either via
 * EXTRA_DEFINES in BOARDS/STM32F407G-DISC1/board.cmake, or by widening the
 * condition below once the generated files are committed.
 */
#if defined(BOARD_STM32F407G_DISC1) && defined(USB_CDC_AVAILABLE)
#define USB_CDC_ENABLED 1
#else
#define USB_CDC_ENABLED 0
#endif

#if USB_CDC_ENABLED
#include "usb_device.h"   /* MX_USB_DEVICE_Init()             */
#include "usbd_cdc_if.h"  /* CDC_Transmit_FS(), USBD_OK, ...  */
#include "usbd_def.h"
extern USBD_HandleTypeDef hUsbDeviceFS;
#endif

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

/** @brief True once @p timeout_ms has elapsed since @p start_ms. Wrap-safe. */
static bool USB_DeadlineExpired(uint32_t start_ms, uint32_t timeout_ms);

static bool USB_DeadlineExpired(uint32_t start_ms, uint32_t timeout_ms)
{
    (void)start_ms;
    (void)timeout_ms;
    return true;
}

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

drv_status_t usb_cdc_init(usb_cdc_t *usb)
{
    if (usb == 0)
    {
        return DRV_ERR_PARAM;
    }

    usb->tx_done_cb = 0;
    usb->tx_done_ctx = 0;
    usb->tx_busy = false;
    usb->tx_dropped = 0;
    usb->initialised = false;

#if USB_CDC_ENABLED
    MX_USB_DEVICE_Init();
    usb->initialised = true;
    return DRV_OK;
#else
    return DRV_ERR_STATE;
#endif
}

bool usb_cdc_is_ready(const usb_cdc_t *usb)
{
#if USB_CDC_ENABLED
    if (usb == 0 || !usb->initialised)
    {
        return false;
    }
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
#else
    (void)usb;
    return false;
#endif
}

/* -------------------------------------------------------------------------- */
/* Transfers                                                                  */
/* -------------------------------------------------------------------------- */

drv_status_t usb_cdc_write(usb_cdc_t *usb, const uint8_t *data, uint16_t len,
                           uint32_t timeout_ms)
{
    if (usb == 0 || (data == 0 && len != 0))
    {
        return DRV_ERR_PARAM;
    }

#if USB_CDC_ENABLED
    if (!usb->initialised)
    {
        return DRV_ERR_STATE;
    }
    if (!usb_cdc_is_ready(usb))
    {
        return DRV_ERR_STATE;
    }
    if (len == 0)
    {
        return DRV_OK;
    }

    /* TODO(impl): retry CDC_Transmit_FS() until it stops returning USBD_BUSY,
     * bounded by timeout_ms via USB_DeadlineExpired(). Map USBD_OK -> DRV_OK,
     * USBD_BUSY at expiry -> DRV_ERR_TIMEOUT, USBD_FAIL -> DRV_ERR_BUS. */
    (void)timeout_ms;
    (void)USB_DeadlineExpired;
    return DRV_ERR_STATE;
#else
    (void)data;
    (void)len;
    (void)timeout_ms;
    (void)USB_DeadlineExpired;
    return DRV_ERR_STATE;
#endif
}

drv_status_t usb_cdc_write_nonblocking(usb_cdc_t *usb, const uint8_t *data,
                                       uint16_t len)
{
    if (usb == 0 || (data == 0 && len != 0))
    {
        return DRV_ERR_PARAM;
    }

#if USB_CDC_ENABLED
    if (!usb->initialised || !usb_cdc_is_ready(usb))
    {
        return DRV_ERR_STATE;
    }
    if (len == 0)
    {
        return DRV_OK;
    }

    /* TODO(impl): single CDC_Transmit_FS() attempt. On USBD_BUSY increment
     * usb->tx_dropped and return DRV_ERR_BUSY without waiting — a stalled host
     * must never stall the control loop. */
    return DRV_ERR_STATE;
#else
    (void)data;
    (void)len;
    return DRV_ERR_STATE;
#endif
}

uint32_t usb_cdc_dropped(const usb_cdc_t *usb)
{
    if (usb == 0)
    {
        return 0;
    }
    return usb->tx_dropped;
}

/* -------------------------------------------------------------------------- */
/* Receive hook                                                               */
/* -------------------------------------------------------------------------- */

void usb_cdc_on_receive(usb_cdc_t *usb, const uint8_t *data, uint32_t len)
{
    (void)usb;
    (void)data;
    (void)len;

    /* TODO(impl): interrupt context. Copy into a ring buffer and return; no
     * parsing, no blocking. Feeds the host command path when that is built. */
}
