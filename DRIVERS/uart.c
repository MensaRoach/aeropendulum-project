#include "uart.h"
#include <stddef.h>

// Define the actual struct for the UART Handle
struct UART_Handle_t {
    // TODO: Add MCU-specific registers/handles (e.g., UART_HandleTypeDef*)
    
    // State tracking for async operations
    UART_Callback_t tx_callback;
    UART_Callback_t rx_callback;
    bool is_tx_busy;
    bool is_rx_busy;
};

bool UART_Init(UART_Handle_t* handle) {
    handle->tx_callback = NULL;
    handle->rx_callback = NULL;
    handle->is_tx_busy = false;
    handle->is_rx_busy = false;
    // TODO: Implement initialization
    return false;
}

bool UART_Read_Poll(UART_Handle_t* handle, uint8_t* data, uint16_t len) {
    // TODO: Implement blocking read
    return false;
}

bool UART_Write_Poll(UART_Handle_t* handle, const uint8_t* data, uint16_t len) {
    // TODO: Implement blocking write
    return false;
}

bool UART_Read_IT(UART_Handle_t* handle, uint8_t* data, uint16_t len, UART_Callback_t cb) {
    if (handle->is_rx_busy) return false;
    handle->rx_callback = cb;
    handle->is_rx_busy = true;
    // TODO: Implement interrupt read
    return true;
}

bool UART_Write_IT(UART_Handle_t* handle, const uint8_t* data, uint16_t len, UART_Callback_t cb) {
    if (handle->is_tx_busy) return false;
    handle->tx_callback = cb;
    handle->is_tx_busy = true;
    // TODO: Implement interrupt write
    return true;
}

bool UART_Read_DMA(UART_Handle_t* handle, uint8_t* data, uint16_t len, UART_Callback_t cb) {
    if (handle->is_rx_busy) return false;
    handle->rx_callback = cb;
    handle->is_rx_busy = true;
    // TODO: Implement DMA read
    return true;
}

bool UART_Write_DMA(UART_Handle_t* handle, const uint8_t* data, uint16_t len, UART_Callback_t cb) {
    if (handle->is_tx_busy) return false;
    handle->tx_callback = cb;
    handle->is_tx_busy = true;
    // TODO: Implement DMA write
    return true;
}

bool UART_Print(UART_Handle_t* handle, const char* str) {
    // TODO: Find the length of the string and call UART_Write_Poll (or an async method if preferred)
    return false;
}
