/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    bus_uart.c
 * @brief   USART driver for STM32F4.
 */

#include "bus_uart.h"

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

drv_status_t uart_bus_init(uart_bus_t *bus)
{
    (void)bus;
    return DRV_ERR_STATE;
}

/* -------------------------------------------------------------------------- */
/* Transfers                                                                  */
/* -------------------------------------------------------------------------- */

drv_status_t uart_bus_write(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                            uint32_t timeout_ms)
{
    (void)bus;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return DRV_ERR_STATE;
}

drv_status_t uart_bus_write_async(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                                  uart_done_fn done, void *user_ctx)
{
    (void)bus;
    (void)data;
    (void)len;
    (void)done;
    (void)user_ctx;
    return DRV_ERR_STATE;
}

drv_status_t uart_bus_read(uart_bus_t *bus, uint8_t *data, uint16_t len,
                           uint32_t timeout_ms)
{
    (void)bus;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return DRV_ERR_STATE;
}

bool uart_bus_is_busy(const uart_bus_t *bus)
{
    (void)bus;
    return false;
}

/* -------------------------------------------------------------------------- */
/* Interrupt handler                                                          */
/* -------------------------------------------------------------------------- */

void uart_bus_on_irq(uart_bus_t *bus)
{
    (void)bus;
}
