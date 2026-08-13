#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

#include "main.h"

// Define your pins here, mapping to the ones generated in main.h

// USART Configuration
#define TELEMETRY_USART USART2

// I2C Configuration
#define MPU6050_I2C I2C1
#define MPU6050_I2C_PORT GPIOB
#define MPU6050_SCL_PIN LL_GPIO_PIN_6
#define MPU6050_SDA_PIN LL_GPIO_PIN_7

#endif // PIN_DEFINITIONS_H
