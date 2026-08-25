#include "mpu6050.h"
#include <stddef.h>

// Global pointer used ONLY to route the callback to the right instance
// (Since the hardware IRQ takes a void(void) callback, we need a way to know 
// which sensor instance finished). 
static MPU6050_t* active_async_sensor = NULL;

bool MPU6050_Init(MPU6050_t* sensor) {
    sensor->data_ready = false;
    
    // TODO: Write sequence to wake up the sensor, set ranges, etc.
    // Use sensor->write_sync(...) to do this.
    
    return true;
}

bool MPU6050_Calibrate(MPU6050_t* sensor) {
    // TODO: Take N readings while still, average them, and store in sensor->gyro_bias
    return true;
}

bool MPU6050_Read_Poll(MPU6050_t* sensor, float* accel_data, float* gyro_data) {
    uint8_t buffer[14];
    
    // 1. Fetch data directly blocking the CPU
    // sensor->read_sync(sensor->bus_handle, sensor->device_address, START_REG, buffer, 14);
    
    // 2. TODO: Convert raw bytes in buffer to physical units and apply bias
    // accel_data[0] = ...
    
    return true;
}

// -------------------------------------------------------------------------
// ASYNC WORKFLOW
// -------------------------------------------------------------------------

// Step 1: Called from your main loop to trigger a background read
bool MPU6050_Start_Read_Async(MPU6050_t* sensor) {
    sensor->data_ready = false;
    active_async_sensor = sensor; // Store instance so the callback knows who finished
    
    // Kick off the background transfer!
    // sensor->read_async(sensor->bus_handle, sensor->device_address, START_REG, sensor->raw_buffer, 14, MPU6050_Async_Complete_Callback);
    
    return true;
}

// Step 2: Called AUTOMATICALLY by the I2C Hardware Interrupt when DMA/IT is done
void MPU6050_Async_Complete_Callback(void) {
    if (active_async_sensor != NULL) {
        // We are in an INTERRUPT CONTEXT! Just set the flag and get out fast.
        active_async_sensor->data_ready = true;
    }
}

// Step 3: Called from your main loop when you see that data_ready == true
bool MPU6050_Process_Async_Data(MPU6050_t* sensor, float* accel_data, float* gyro_data) {
    if (!sensor->data_ready) {
        return false; // Data isn't here yet!
    }
    
    // TODO: Convert the raw bytes in sensor->raw_buffer to physical units
    // accel_data[0] = ...
    
    // Reset the flag for the next cycle
    sensor->data_ready = false;
    
    return true;
}
