#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// Callback function pointer type for UART async operations
typedef void (*UART_Callback_t)(void);

// Opaque pointer for the UART hardware instance
typedef struct UART_Handle_t UART_Handle_t;

// Initialization
bool UART_Init(UART_Handle_t* handle);

// Polling (Blocking) Transfers
bool UART_Read_Poll(UART_Handle_t* handle, uint8_t* data, uint16_t len);
bool UART_Write_Poll(UART_Handle_t* handle, const uint8_t* data, uint16_t len);

// Interrupt (Non-Blocking) Transfers
bool UART_Read_IT(UART_Handle_t* handle, uint8_t* data, uint16_t len, UART_Callback_t cb);
bool UART_Write_IT(UART_Handle_t* handle, const uint8_t* data, uint16_t len, UART_Callback_t cb);

// DMA (Non-Blocking) Transfers
bool UART_Read_DMA(UART_Handle_t* handle, uint8_t* data, uint16_t len, UART_Callback_t cb);
bool UART_Write_DMA(UART_Handle_t* handle, const uint8_t* data, uint16_t len, UART_Callback_t cb);

// Helper function for printing null-terminated strings
bool UART_Print(UART_Handle_t* handle, const char* str);

#endif // UART_DRIVER_H
