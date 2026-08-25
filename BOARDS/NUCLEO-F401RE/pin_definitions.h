#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

#include "main.h"

// Define your pins here, mapping to the ones generated in main.h

// USART Configuration
#define TELEMETRY_USART USART2

// Status LED Configuration (LD2 on the NUCLEO-F401RE)
// Not configured by CubeMX: MX_GPIO_Init() leaves PA5 in analog mode,
// so the application must reconfigure it as an output before use.
#define STATUS_LED_PORT GPIOA
#define STATUS_LED_PIN LL_GPIO_PIN_5

// IMU I2C bus (I2C1 on PB6/PB7, AF4, open-drain, configured by MX_I2C1_Init()).
//
// The GPIO aliases are here for bus recovery only: a slave left holding SDA low
// across an MCU reset has to be clocked free by bit-banging the pins before the
// peripheral is enabled. Normal traffic never touches them.
//
// Note that MX_I2C1_Init() leaves the peripheral DISABLED -- LL_I2C_Init() ends
// with LL_I2C_Disable() and never re-enables. Enabling it is i2c_bus_init()'s job.
#define IMU_I2C I2C1
#define IMU_I2C_PORT GPIOB
#define IMU_I2C_SCL_PIN LL_GPIO_PIN_6
#define IMU_I2C_SDA_PIN LL_GPIO_PIN_7
#define IMU_I2C_AF LL_GPIO_AF_4

#endif // PIN_DEFINITIONS_H
