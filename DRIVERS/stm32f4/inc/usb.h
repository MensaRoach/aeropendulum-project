/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    usb.h
 * @brief   USB CDC (virtual COM port) transport for STM32F4.
 *
 * A host-link alternative to ::uart_bus_t for boards whose USART does not reach
 * the PC. The STM32F407G-DISC1 exposes USB OTG FS on its "USB User" connector
 * (PA11/PA12) but does not route a USART to the ST-LINK, so USB CDC is the
 * practical way to get telemetry off that board.
 *
 * The API mirrors bus_uart.h deliberately: same shape, same status codes, same
 * timeout discipline. An application that prints through one can switch to the
 * other by changing which init it calls.
 *
 * @par Prerequisite — CubeMX must generate the device stack first
 * This file is a thin wrapper, not a USB stack. It calls into ST's USB Device
 * middleware, which CubeMX generates:
 *
 *   1. Connectivity -> USB_OTG_FS -> Mode: Device_Only
 *   2. Middleware -> USB_DEVICE -> Class: Communication Device Class (Virtual Port Com)
 *   3. Clock config: USB OTG FS needs a 48 MHz clock — CubeMX will flag this if
 *      the PLL is not producing one
 *   4. Generate (LL for peripherals; the USB middleware is HAL-based by design,
 *      which is expected and does not violate the LL-only convention for
 *      peripheral drivers)
 *
 * That produces `USB_DEVICE/App/usb_device.c`, `usbd_cdc_if.c`, `usbd_desc.c`
 * and `usbd_conf.c`, and with them `MX_USB_DEVICE_Init()` and
 * `CDC_Transmit_FS()`. Until then this driver compiles to stubs that return
 * ::DRV_ERR_STATE — it will never silently pretend to have sent anything.
 *
 * @par Enabling
 * Guarded by @c USB_CDC_AVAILABLE, which the build defines only for boards that
 * have the middleware generated. See DRIVERS/stm32f4/src/usb.c.
 */

#ifndef USB_H
#define USB_H

#include <stdbool.h>
#include <stdint.h>

#include "drv_status.h"

/** @brief Transmit completion callback. Runs in interrupt context. */
typedef void (*usb_done_fn)(void *user_ctx, drv_status_t status);

/**
 * @brief USB CDC link instance.
 *
 * No configuration fields: unlike a USART there is no instance or pin to pick,
 * because the device stack owns the peripheral. Declare one and initialise it.
 */
typedef struct
{
    /** @cond INTERNAL */
    usb_done_fn tx_done_cb;
    void *tx_done_ctx;
    volatile bool tx_busy;
    volatile uint32_t tx_dropped;
    bool initialised;
    /** @endcond */
} usb_cdc_t;

/**
 * @brief Start the USB device stack and prepare the handle.
 *
 * Calls `MX_USB_DEVICE_Init()`. Enumeration is asynchronous — this returning
 * ::DRV_OK means the device started, not that a host has connected yet. Use
 * ::usb_cdc_is_ready to test for an open port.
 *
 * @retval DRV_ERR_STATE Built without the CubeMX USB middleware present.
 */
drv_status_t usb_cdc_init(usb_cdc_t *usb);

/**
 * @brief Whether the host has enumerated the device and opened the port.
 *
 * Always check before treating a write failure as an error: "no terminal open
 * yet" is a normal state, not a fault.
 */
bool usb_cdc_is_ready(const usb_cdc_t *usb);

/**
 * @brief Send @p len bytes, blocking until the stack accepts them.
 *
 * CDC transmission is buffered by the device stack, so this returns once the
 * data is queued, not once it reaches the host.
 *
 * @retval DRV_OK          Queued for transmission.
 * @retval DRV_ERR_BUSY    Previous transfer still in flight past the deadline.
 * @retval DRV_ERR_STATE   Not initialised, or no host connected.
 * @retval DRV_ERR_TIMEOUT Deadline expired waiting for the endpoint to free.
 */
drv_status_t usb_cdc_write(usb_cdc_t *usb, const uint8_t *data, uint16_t len,
                           uint32_t timeout_ms);

/**
 * @brief Send without blocking. Drops the payload if the endpoint is busy.
 *
 * For periodic telemetry, where a stalled host must never stall the control
 * loop. Increments the counter reported by ::usb_cdc_dropped rather than
 * waiting.
 */
drv_status_t usb_cdc_write_nonblocking(usb_cdc_t *usb, const uint8_t *data,
                                       uint16_t len);

/**
 * @brief Frames discarded by ::usb_cdc_write_nonblocking because the endpoint
 *        was busy.
 *
 * A rising count means the link cannot keep up with the send rate. Silence here
 * is the difference between "telemetry looks sparse" and knowing why.
 */
uint32_t usb_cdc_dropped(const usb_cdc_t *usb);

/**
 * @brief Hand received bytes to the driver.
 *
 * Call from `CDC_Receive_FS()` in the generated `usbd_cdc_if.c`, inside its
 * USER CODE block — the same pattern the I2C and USART drivers use for IRQs, so
 * regenerating from CubeMX cannot clobber the wiring.
 */
void usb_cdc_on_receive(usb_cdc_t *usb, const uint8_t *data, uint32_t len);

#endif /* USB_H */
