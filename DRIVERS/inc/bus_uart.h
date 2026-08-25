/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    bus_uart.h
 * @brief   USART driver for STM32F4.
 *
 * @par Frame format
 * The port is configured for 8 data bits with even parity, which the peripheral
 * carries as a 9-bit word. Transmission inserts the parity bit in hardware.
 * Reception returns it in the top bit, so received words must be masked and the
 * parity error flag checked.
 */

#ifndef BUS_UART_H
#define BUS_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_ll_usart.h"

#include "drv_status.h"

/** @brief Transmit completion callback. Runs in interrupt context. */
typedef void (*uart_done_fn)(void *user_ctx, drv_status_t status);

/**
 * @brief USART instance.
 *
 * Populate @c instance before calling ::uart_bus_init.
 */
typedef struct
{
    USART_TypeDef *instance; /**< Peripheral instance, e.g. `USART2`. */

    /** @cond INTERNAL */
    uart_done_fn tx_done_cb;
    void *tx_done_ctx;
    volatile bool tx_busy;
    bool initialised;
    /** @endcond */
} uart_bus_t;

/**
 * @brief Enable the peripheral.
 *
 * Assumes CubeMX-generated code has already configured baud rate, word length
 * and parity.
 */
drv_status_t uart_bus_init(uart_bus_t *bus);

/** @brief Transmit, blocking until the last bit has left the shift register. */
drv_status_t uart_bus_write(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                            uint32_t timeout_ms);

/**
 * @brief Transmit without blocking.
 *
 * @p data must remain valid until @p done fires.
 */
drv_status_t uart_bus_write_async(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                                  uart_done_fn done, void *user_ctx);

/** @brief Receive @p len bytes, blocking. Strips parity and reports frame errors. */
drv_status_t uart_bus_read(uart_bus_t *bus, uint8_t *data, uint16_t len,
                           uint32_t timeout_ms);

/** @brief Whether a transmission is in progress. */
bool uart_bus_is_busy(const uart_bus_t *bus);

/** @brief Call from the USART `USER CODE` block in `stm32f4xx_it.c`. */
void uart_bus_on_irq(uart_bus_t *bus);

#endif /* BUS_UART_H */
