#ifndef BUS_UART_H
#define BUS_UART_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32f4xx_ll_usart.h"

#include "drv_status.h"

// ==============================================================================
// STM32F4 USART driver (platform layer)
// ==============================================================================
// Scope note: nothing in the driver development plan calls for this yet.
// APPS/src/hello_usart.c already does blocking TX directly, and RX is only needed
// once the host command path exists -- which the plan lists as an explicit
// "decide whether to build now or defer" item, still undecided.
//
// It is kept here, reshaped to match bus_i2c.h, so the two platform drivers look
// like each other. If the command path gets deferred, delete this pair rather
// than let it accumulate more stubs.
//
// ------------------------------------------------------------------------------
// Wire format caution
// ------------------------------------------------------------------------------
// CubeMX configures USART2 as DATAWIDTH_9B + PARITY_EVEN, which is 8 data bits
// plus a parity bit on the wire (115200 8E1). On transmit the hardware inserts
// parity, so writing plain bytes works. On RECEIVE, a 9-bit read returns the
// parity bit in the top position -- it has to be masked off, and parity errors
// have to be checked rather than silently folded into the data.
//
// That asymmetry is why read and write are not symmetrical here.

typedef struct
{
    // --- set by the caller before uart_bus_init() ---------------------------
    USART_TypeDef *instance;   // e.g. TELEMETRY_USART from pin_definitions.h

    // --- private: owned by bus_uart.c ---------------------------------------
    void (*tx_done_cb)(void *user_ctx, drv_status_t status);
    void *tx_done_ctx;
    volatile bool tx_busy;
    bool initialised;
} uart_bus_t;

// Enables the peripheral. Assumes MX_USART2_UART_Init() has already configured it
// -- same ownership split as bus_i2c.h: CubeMX configures, this driver enables.
drv_status_t uart_bus_init(uart_bus_t *bus);

// Blocking transmit with a timeout, for the same reason the I2C path has one: an
// unbounded spin on TXE hangs the firmware if the peripheral never drains.
drv_status_t uart_bus_write(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                            uint32_t timeout_ms);

// Non-blocking transmit. Whether the implementation uses the TXE interrupt or DMA
// is an implementation detail and deliberately not in the signature.
drv_status_t uart_bus_write_async(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                                  void (*done)(void *user_ctx, drv_status_t status),
                                  void *user_ctx);

// Blocking receive. Deferred -- see the scope note above. Returns DRV_ERR_STATE
// until the host command path is actually decided on.
drv_status_t uart_bus_read(uart_bus_t *bus, uint8_t *data, uint16_t len,
                           uint32_t timeout_ms);

bool uart_bus_is_busy(const uart_bus_t *bus);

// Call from the USER CODE block of the matching handler in Core/Src/stm32f4xx_it.c.
// USART2_IRQn is not currently enabled in the .ioc.
void uart_bus_on_irq(uart_bus_t *bus);

#endif // BUS_UART_H
