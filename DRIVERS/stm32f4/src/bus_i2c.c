/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    bus_i2c.c
 * @brief   I2C master bus driver for STM32F4 (I2C v1 peripheral).
 */

#include "bus_i2c.h"

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

/** @brief True once @p timeout_ms has elapsed since @p start_ms. */
static bool I2C_DeadlineExpired(uint32_t start_ms, uint32_t timeout_ms);

/** @brief Map the peripheral error flags onto a status code and clear them. */
static drv_status_t I2C_TranslateError(I2C_TypeDef *instance);

static bool I2C_DeadlineExpired(uint32_t start_ms, uint32_t timeout_ms)
{
    (void)start_ms;
    (void)timeout_ms;
    return true;
}

static drv_status_t I2C_TranslateError(I2C_TypeDef *instance)
{
    (void)instance;
    return DRV_ERR_STATE;
}

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

drv_status_t i2c_bus_init(i2c_bus_t *bus, i2c_speed_t speed)
{
    (void)bus;
    (void)speed;
    return DRV_ERR_STATE;
}

drv_status_t i2c_bus_recover(i2c_bus_t *bus)
{
    (void)bus;
#if defined(BOARD_NUCLEO_F401RE)
#elif defined(BOARD_STM32F407G_DISC1)
#else
#error "Unsupported board — no BOARD_* macro defined. See add_stm32_board() in .cmake/stm32.cmake."
#endif
    return DRV_ERR_STATE;
}

/* -------------------------------------------------------------------------- */
/* Blocking transfers                                                         */
/* -------------------------------------------------------------------------- */

drv_status_t i2c_bus_read(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                          uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return DRV_ERR_STATE;
}

drv_status_t i2c_bus_write(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                           const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return DRV_ERR_STATE;
}

/* -------------------------------------------------------------------------- */
/* Non-blocking transfers                                                     */
/* -------------------------------------------------------------------------- */

drv_status_t i2c_bus_read_async(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len,
                                i2c_done_fn done, void *user_ctx)
{
    (void)bus;
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    (void)done;
    (void)user_ctx;
    return DRV_ERR_STATE;
}

bool i2c_bus_is_busy(const i2c_bus_t *bus)
{
    (void)bus;
    return false;
}

/* -------------------------------------------------------------------------- */
/* Transport binding                                                          */
/* -------------------------------------------------------------------------- */

i2c_transport_t i2c_bus_transport(i2c_bus_t *bus)
{
    i2c_transport_t transport;

    transport.bus = bus;
    transport.read = i2c_bus_read;
    transport.write = i2c_bus_write;
    transport.read_async = i2c_bus_read_async;

    return transport;
}

/* -------------------------------------------------------------------------- */
/* Interrupt handlers                                                         */
/* -------------------------------------------------------------------------- */

void i2c_bus_on_event_irq(i2c_bus_t *bus)
{
    (void)bus;
    (void)I2C_DeadlineExpired;
}

void i2c_bus_on_error_irq(i2c_bus_t *bus)
{
    (void)bus;
    (void)I2C_TranslateError;
}

void i2c_bus_on_dma_rx_irq(i2c_bus_t *bus)
{
    (void)bus;
}

void i2c_bus_on_dma_tx_irq(i2c_bus_t *bus)
{
    (void)bus;
}
