#include "bus_uart.h"

// ==============================================================================
// STM32F4 USART driver -- SCAFFOLD ONLY
// ==============================================================================
// See the scope note in bus_uart.h: nothing on the plan needs this yet.
// APPS/src/hello_usart.c already transmits directly, and RX waits on the host
// command path decision. Kept only so the two platform drivers mirror each other.

drv_status_t uart_bus_init(uart_bus_t *bus)
{
    (void)bus;

    // TODO: same ownership split as I2C -- MX_USART2_UART_Init() has configured
    // the peripheral; check whether LL_USART_Init leaves it enabled or disabled
    // before assuming either way, and do whatever is actually left over.
    return DRV_ERR_STATE;
}

drv_status_t uart_bus_write(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                            uint32_t timeout_ms)
{
    (void)bus;
    (void)data;
    (void)len;
    (void)timeout_ms;

    // TODO: the blocking TX loop already exists in APPS/src/hello_usart.c and in
    // the deleted telemetry app. Porting it is mostly adding the deadline.
    // Remember TC, not TXE, is the flag that means the last bit has left the pin.
    return DRV_ERR_STATE;
}

drv_status_t uart_bus_write_async(uart_bus_t *bus, const uint8_t *data, uint16_t len,
                                  void (*done)(void *user_ctx, drv_status_t status),
                                  void *user_ctx)
{
    (void)bus;
    (void)data;
    (void)len;
    (void)done;
    (void)user_ctx;

    // TODO: deferred. USART2_IRQn is not enabled in the .ioc, and no DMA request
    // is configured for USART2 either.
    return DRV_ERR_STATE;
}

drv_status_t uart_bus_read(uart_bus_t *bus, uint8_t *data, uint16_t len,
                           uint32_t timeout_ms)
{
    (void)bus;
    (void)data;
    (void)len;
    (void)timeout_ms;

    // TODO: deferred until the host command path is decided.
    //
    // When it is: the port runs 9-bit + even parity, so a received frame carries
    // the parity bit above the data. Mask it off and check the parity error flag.
    // Folding a parity bit into the data silently corrupts exactly the frames
    // that were already damaged -- and COBS will then fail to decode them in a
    // way that looks like a framing bug rather than a line problem.
    return DRV_ERR_STATE;
}

bool uart_bus_is_busy(const uart_bus_t *bus)
{
    (void)bus;
    return false;
}

void uart_bus_on_irq(uart_bus_t *bus)
{
    (void)bus;
    // TODO: deferred alongside uart_bus_write_async().
}
