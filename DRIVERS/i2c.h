#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// Define the callback function pointer type.
// This is the "phone number" the higher layer gives to the I2C driver.
typedef void (*I2C_Callback_t)(void);

// Forward declaration of the platform-specific I2C handle
typedef struct I2C_Handle_t I2C_Handle_t;

// Initialization
bool I2C_Init(I2C_Handle_t* handle);

// Polling (Blocking) Transfers - No callback needed because they don't return until done
bool I2C_Read_Poll(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len);
bool I2C_Write_Poll(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, uint16_t len);

// Interrupt (Non-Blocking) Transfers - Take a callback to execute when finished
bool I2C_Read_IT(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len, I2C_Callback_t cb);
bool I2C_Write_IT(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, uint16_t len, I2C_Callback_t cb);

// DMA (Non-Blocking) Transfers - Take a callback to execute when finished
bool I2C_Read_DMA(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len, I2C_Callback_t cb);
bool I2C_Write_DMA(I2C_Handle_t* handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, uint16_t len, I2C_Callback_t cb);

#endif // I2C_DRIVER_H
