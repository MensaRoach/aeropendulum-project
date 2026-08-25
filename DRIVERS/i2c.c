#include "i2c.h"
#include <stddef.h>

struct I2C_Handle_t {
    // TODO: Add your MCU-specific registers or handles here
    // (e.g., I2C_TypeDef* instance)
    
    // State tracking for async operations
    I2C_Callback_t current_callback; // Stores the callback to fire when done
    bool is_busy;                    // Prevents starting a new transfer while one is running
};

bool I2C_Init(I2C_Handle_t* handle) {
    handle->current_callback = NULL;
    handle->is_busy = false;
    // TODO: Implement initialization
    return false;
}

bool I2C_Read_Poll(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len) {
    // TODO: Implement blocking read
    return false;
}

bool I2C_Write_Poll(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, uint16_t len) {
    // TODO: Implement blocking write
    return false;
}

bool I2C_Read_IT(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len, I2C_Callback_t cb) {
    if (handle->is_busy) return false;
    
    handle->current_callback = cb;
    handle->is_busy = true;
    
    // TODO: Configure STM32 interrupts and start the transfer
    return true;
}

bool I2C_Write_IT(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, uint16_t len, I2C_Callback_t cb) {
    if (handle->is_busy) return false;
    
    handle->current_callback = cb;
    handle->is_busy = true;
    
    // TODO: Configure STM32 interrupts and start the transfer
    return true;
}

bool I2C_Read_DMA(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len, I2C_Callback_t cb) {
    if (handle->is_busy) return false;
    
    handle->current_callback = cb;
    handle->is_busy = true;
    
    // TODO: Configure STM32 DMA and start the transfer
    return true;
}

bool I2C_Write_DMA(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, uint16_t len, I2C_Callback_t cb) {
    if (handle->is_busy) return false;
    
    handle->current_callback = cb;
    handle->is_busy = true;
    
    // TODO: Configure STM32 DMA and start the transfer
    return true;
}

/* 
 * ====================================================================
 * HARDWARE INTERRUPT HANDLER (Example)
 * ====================================================================
 * When you eventually implement the STM32 hardware code, you will have 
 * an IRQ handler that looks something like this. This is where you 
 * actually execute the stored callback!
 */
 
// void I2C1_EV_IRQHandler(void) { // Or DMA1_StreamX_IRQHandler
//     // 1. Clear hardware interrupt flags...
//     
//     // 2. Mark the handle as free so new transfers can start
//     // my_i2c_handle.is_busy = false;
//     
//     // 3. Call the MPU6050's callback if one was provided
//     // if (my_i2c_handle.current_callback != NULL) {
//     //     my_i2c_handle.current_callback();
//     // }
// }
