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

// I2C1 is declared in the .ioc and initialised by MX_I2C1_Init(), but no
// application-level alias is defined for it yet.

#endif // PIN_DEFINITIONS_H
