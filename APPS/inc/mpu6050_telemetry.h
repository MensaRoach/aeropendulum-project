#ifndef MPU6050_TELEMETRY_H
#define MPU6050_TELEMETRY_H

#include <stdint.h>

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} __attribute__((packed)) MPU6050_Data_t;

void mpu6050_telemetry_init(void);
void mpu6050_telemetry_loop(void);

#endif // MPU6050_TELEMETRY_H
