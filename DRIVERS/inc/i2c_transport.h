/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    i2c_transport.h
 * @brief   Abstract I2C register-access interface.
 *
 * Device drivers depend on this header; platform drivers implement it. Neither
 * includes the other, so a device driver carries no MCU dependency and can be
 * exercised against a mock transport on the host.
 *
 * @note Device addresses are 7-bit and right-aligned throughout this API. The
 *       implementation appends the read/write bit.
 */

#ifndef I2C_TRANSPORT_H
#define I2C_TRANSPORT_H

#include <stdint.h>

#include "drv_status.h"

/**
 * @brief Async transfer completion callback. Runs in interrupt context.
 *
 * @param user_ctx Opaque pointer supplied when the transfer was started.
 * @param status   Result of the transfer.
 */
typedef void (*i2c_done_fn)(void *user_ctx, drv_status_t status);

/**
 * @brief Blocking register read.
 *
 * @param bus        Implementation handle.
 * @param dev_addr   7-bit device address.
 * @param reg_addr   Register to read from.
 * @param data       Destination buffer.
 * @param len        Number of bytes to read.
 * @param timeout_ms Deadline for the whole transfer.
 */
typedef drv_status_t (*i2c_read_fn)(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                                    uint8_t *data, uint16_t len, uint32_t timeout_ms);

/** @brief Blocking register write. Parameters mirror ::i2c_read_fn. */
typedef drv_status_t (*i2c_write_fn)(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                                     const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Non-blocking register read.
 *
 * Returns as soon as the transfer is started. @p data belongs to the transport
 * until @p done fires and must not be touched in the meantime.
 */
typedef drv_status_t (*i2c_read_async_fn)(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                                          uint8_t *data, uint16_t len,
                                          i2c_done_fn done, void *user_ctx);

/** @brief Bus interface handed to a device driver at initialisation. */
typedef struct
{
    void *bus;                    /**< Passed back as the first argument of every call. */
    i2c_read_fn read;
    i2c_write_fn write;
    i2c_read_async_fn read_async; /**< NULL if the bus has no async path. */
} i2c_transport_t;

#endif /* I2C_TRANSPORT_H */
