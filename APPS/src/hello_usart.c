#include "hello_usart.h"
#include "main.h" // For LL drivers
#include "pin_definitions.h"

void hello_usart_init(void)
{
    // USART2 is already initialized by MX_USART2_UART_Init() in main.c
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
