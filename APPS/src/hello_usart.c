#include "hello_usart.h"
#include "main.h" // For LL drivers
#include "pin_definitions.h"

/* hello_usart requires TELEMETRY_USART to be defined in pin_definitions.h.
 * On boards that have not yet configured a USART (e.g. STM32F407G-DISC1 before
 * the collaborator adds one to the .ioc), the functions compile as no-ops.
 * APP_HELLO_USART must not be selected on such boards until TELEMETRY_USART
 * is defined and a USART is initialised by CubeMX. */
#ifdef TELEMETRY_USART

void hello_usart_init(void)
{
    // USART is already initialised by the MX_USART*_UART_Init() call in main.c
}

void hello_usart_loop(void)
{
    const char *msg = "Hello\r\n";
    const char *p = msg;

    // Send each character
    while (*p)
    {
        // Wait until TXE (Transmit Data Register Empty) flag is set
        while (!LL_USART_IsActiveFlag_TXE(TELEMETRY_USART))
        {
        }

        // Write character to data register
        LL_USART_TransmitData8(TELEMETRY_USART, *p++);
    }

    // Wait until TC (Transmission Complete) flag is set
    while (!LL_USART_IsActiveFlag_TC(TELEMETRY_USART))
    {
    }

    // Simple software delay
    for (volatile uint32_t i = 0; i < 2000000; i++)
    {
    }
}

#else /* TELEMETRY_USART not defined — board has no USART configured yet */

void hello_usart_init(void) {}
void hello_usart_loop(void) {}

#endif /* TELEMETRY_USART */

