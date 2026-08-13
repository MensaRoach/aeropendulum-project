#include "hello_usart.h"
#include "main.h" // For USART2 definition and LL drivers

void hello_usart_init(void) {
    // USART2 is already initialized by MX_USART2_UART_Init() in main.c
}

void hello_usart_loop(void) {
    const char *msg = "Hello\r\n";
    const char *p = msg;
    
    // Send each character
    while (*p) {
        // Wait until TXE (Transmit Data Register Empty) flag is set
        while (!LL_USART_IsActiveFlag_TXE(USART2)) {}
        
        // Write character to data register
        LL_USART_TransmitData8(USART2, *p++);
    }
    
    // Wait until TC (Transmission Complete) flag is set
    while (!LL_USART_IsActiveFlag_TC(USART2)) {}
    
    // Simple software delay
    for (volatile uint32_t i = 0; i < 2000000; i++) {}
}
