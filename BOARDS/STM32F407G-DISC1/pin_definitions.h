#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

#include "main.h"

/* -----------------------------------------------------------------------
 * STM32F407G-DISC1 pin definitions
 *
 * TELEMETRY_USART is intentionally absent: the .ioc does not declare a
 * USART yet. Add it after the collaborator configures one in CubeMX with
 * LL drivers and regenerates the BSP. Verify the ST-LINK VCP wiring before
 * assuming USART output routes to the host; the DISC1 may need a USB-TTL
 * adapter on a different USART than the Nucleo. See the Phase 1 notes in
 * .docs/llm-plans/2026-08-26-0140/multi-board-restructure.md §6.4.
 * ----------------------------------------------------------------------- */

/* Status LED — LD4 (green) on PD12.
 * The DISC1 has four user LEDs: LD3 (orange, PD13), LD4 (green, PD12),
 * LD5 (red, PD14), LD6 (blue, PD15). LD4 is chosen to match the "alive"
 * colour convention used by APP_BLINKY on the Nucleo (LD2 is also green).
 *
 * MX_GPIO_Init() leaves these pins in analog mode (not declared in the
 * .ioc). The application must call LL_GPIO_Init() before toggling.
 * GPIOD's clock is enabled directly by MX_GPIO_Init() (LL_AHB1_GRP1_EnableClock
 * for GPIOD), the same as every other port it configures. */
#define STATUS_LED_PORT GPIOD
#define STATUS_LED_PIN  LL_GPIO_PIN_12

/* IMU I2C bus — I2C1 on PB6 (SCL) / PB7 (SDA), AF4, open-drain.
 * Identical pin assignment to the NUCLEO-F401RE. Configured by
 * MX_I2C1_Init(), which leaves the peripheral DISABLED (LL_I2C_Disable()
 * at the end of LL_I2C_Init()). i2c_bus_init() must enable it.
 *
 * The GPIO aliases exist for bit-banged bus recovery only; normal traffic
 * goes through the peripheral. */
#define IMU_I2C         I2C1
#define IMU_I2C_PORT    GPIOB
#define IMU_I2C_SCL_PIN LL_GPIO_PIN_6
#define IMU_I2C_SDA_PIN LL_GPIO_PIN_7
#define IMU_I2C_AF      LL_GPIO_AF_4

#endif /* PIN_DEFINITIONS_H */