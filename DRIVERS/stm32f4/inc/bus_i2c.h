/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    bus_i2c.h
 * @brief   I2C master bus driver for STM32F4 (I2C v1 peripheral).
 *
 * Implements ::i2c_transport_t. One ::i2c_bus_t instance represents one I2C
 * peripheral and is shared by every device on that bus.
 *
 * @par Ownership
 * The peripheral is configured by CubeMX-generated code (`MX_I2C1_Init()`),
 * which leaves it disabled. This driver owns enabling it, bus recovery, and all
 * transfers. Devices never manage the bus lifecycle.
 *
 * @par Async support
 * ::i2c_bus_read_async requires the I2C event and error interrupts to be
 * enabled. On the I2C v1 peripheral, DMA transfers the data phase only; the
 * address phase is driven from the event interrupt.
 */

#ifndef BUS_I2C_H
#define BUS_I2C_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_i2c.h"

#include "drv_status.h"
#include "i2c_transport.h"

/** @brief SCL frequency. */
typedef enum
{
    I2C_SPEED_STANDARD_100K,
    I2C_SPEED_FAST_400K
} i2c_speed_t;

/**
 * @brief I2C bus instance.
 *
 * Populate the configuration fields before calling ::i2c_bus_init. The
 * remaining fields are managed by the driver.
 */
typedef struct
{
    /** @name Configuration */
    /**@{*/
    I2C_TypeDef *instance;   /**< Peripheral instance, e.g. `I2C1`. */
    GPIO_TypeDef *gpio_port; /**< Port carrying SCL and SDA. */
    uint32_t scl_pin;        /**< SCL pin mask. Used for bus recovery. */
    uint32_t sda_pin;        /**< SDA pin mask. Used for bus recovery. */
    uint32_t gpio_af;        /**< Alternate function to restore after recovery. */
    /**@}*/

    /** @cond INTERNAL */
    i2c_done_fn done_cb;
    void *done_ctx;
    uint8_t *rx_ptr;
    uint16_t rx_len;
    uint8_t pending_dev_addr;
    uint8_t pending_reg_addr;
    volatile bool busy;
    volatile drv_status_t last_status;
    bool initialised;
    /** @endcond */
} i2c_bus_t;

/**
 * @brief Recover the bus and enable the peripheral.
 *
 * On failure the peripheral is left disabled and every transfer function
 * returns ::DRV_ERR_STATE until a later call succeeds.
 *
 * @param bus   Bus instance with its configuration fields populated.
 * @param speed SCL frequency to apply.
 */
drv_status_t i2c_bus_init(i2c_bus_t *bus, i2c_speed_t speed);

/**
 * @brief Clock a stuck slave off the bus and restore the pins.
 *
 * A slave interrupted mid-byte can hold SDA low indefinitely, which blocks the
 * peripheral before it is even enabled. Bit-bangs SCL until SDA is released,
 * issues a stop condition, then returns the pins to alternate-function mode.
 */
drv_status_t i2c_bus_recover(i2c_bus_t *bus);

/** @brief Blocking register read. See ::i2c_read_fn. */
drv_status_t i2c_bus_read(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                          uint8_t *data, uint16_t len, uint32_t timeout_ms);

/** @brief Blocking register write. See ::i2c_write_fn. */
drv_status_t i2c_bus_write(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                           const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/** @brief Non-blocking register read. See ::i2c_read_async_fn. */
drv_status_t i2c_bus_read_async(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len,
                                i2c_done_fn done, void *user_ctx);

/** @brief Whether a transfer is currently in flight. */
bool i2c_bus_is_busy(const i2c_bus_t *bus);

/** @brief Build the device-facing view of this bus. */
i2c_transport_t i2c_bus_transport(i2c_bus_t *bus);

/**
 * @name Interrupt handlers
 * Call from the corresponding `USER CODE` block in `stm32f4xx_it.c`.
 */
/**@{*/
void i2c_bus_on_event_irq(i2c_bus_t *bus);
void i2c_bus_on_error_irq(i2c_bus_t *bus);
void i2c_bus_on_dma_rx_irq(i2c_bus_t *bus);
void i2c_bus_on_dma_tx_irq(i2c_bus_t *bus);
/**@}*/

#endif /* BUS_I2C_H */
