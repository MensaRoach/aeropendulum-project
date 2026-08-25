#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// -------------------------------------------------------------------------
// HARDWARE AGNOSTIC INTERFACE
// These function pointers define how the MPU talks to the outside world.
// We use `void*` for the bus_handle so the MPU driver doesn't need to `#include "i2c.h"`.
// -------------------------------------------------------------------------
typedef bool (*MPU6050_Read_Func)(void* bus_handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len);
typedef bool (*MPU6050_Write_Func)(void* bus_handle, uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, uint16_t len);

// Same for the async versions which require a callback
typedef void (*MPU6050_Callback_t)(void);
typedef bool (*MPU6050_Read_Async_Func)(void* bus_handle, uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint16_t len, MPU6050_Callback_t cb);

// -------------------------------------------------------------------------
// MPU6050 CLASS STRUCT
// -------------------------------------------------------------------------
typedef struct {
    // Dependency Injection (The Hardware Link)
    void* bus_handle;                  // Pointer to your I2C_Handle_t
    MPU6050_Read_Func read_sync;       // Pointer to I2C_Read_Poll
    MPU6050_Write_Func write_sync;     // Pointer to I2C_Write_Poll
    MPU6050_Read_Async_Func read_async;// Pointer to I2C_Read_IT or _DMA
    
    // Sensor State
    uint8_t device_address;
    float gyro_bias[3];
    float accel_bias[3];
    
    // Internal Buffer for Async reads
    uint8_t raw_buffer[14]; 
    
    // Flag set by the interrupt callback (MUST BE VOLATILE!)
    volatile bool data_ready;          
} MPU6050_t;

// -------------------------------------------------------------------------
// METHODS
// -------------------------------------------------------------------------

// Initialization
bool MPU6050_Init(MPU6050_t* sensor);

// Calibration (calculates and stores biases)
bool MPU6050_Calibrate(MPU6050_t* sensor);

// Option A: Blocking Read (Pauses CPU until done)
bool MPU6050_Read_Poll(MPU6050_t* sensor, float* accel_data, float* gyro_data);

// Option B: Non-Blocking Read Workflow
bool MPU6050_Start_Read_Async(MPU6050_t* sensor);
void MPU6050_Async_Complete_Callback(void); // Triggered by the I2C interrupt
bool MPU6050_Process_Async_Data(MPU6050_t* sensor, float* accel_data, float* gyro_data);

#endif // MPU6050_DRIVER_H
