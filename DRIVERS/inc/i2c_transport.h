#ifndef I2C_TRANSPORT_H
#define I2C_TRANSPORT_H

#include <stdint.h>
#include "drv_status.h"

// ==============================================================================
// I2C transport interface (the hardware-agnostic seam)
// ==============================================================================
// This header is the *only* thing a device driver (mpu6050.h, ...) is allowed to
// know about the bus. It names no MCU, includes no LL header, and never includes
// bus_i2c.h.
//
// Dependency direction:
//
//     mpu6050.h  --includes-->  i2c_transport.h  <--implements--  bus_i2c.c
//
// bus_i2c.c provides adapter functions whose signatures match the typedefs below
// exactly, so no function-pointer casts are ever needed. If you find yourself
// writing a cast to populate one of these fields, the signatures have drifted
// apart and one of the two sides is wrong.
//
// ------------------------------------------------------------------------------
// Address convention
// ------------------------------------------------------------------------------
// dev_addr is a 7-BIT address, right-aligned (i.e. the value in the range 0x00 to
// 0x7F, NOT pre-shifted, and NOT carrying an R/W bit). The bus implementation is
// responsible for shifting it and appending R/W.
//
// Fix this convention here, once, and state it in every doc comment. Mixing
// 7-bit and 8-bit addresses is the single most common I2C bring-up bug, and it
// presents as a total silence that looks identical to a wiring fault.

// ------------------------------------------------------------------------------
// Async completion callback
// ------------------------------------------------------------------------------
// Called from interrupt context when an async transfer finishes.
//
// user_ctx is whatever the caller handed to the async call. It exists so that a
// device driver never needs a file-scope "which instance is active?" pointer:
// that pattern silently breaks the moment two devices share the bus (an MPU6050
// pair on AD0-low / AD0-high is the obvious case).
//
// status reports how the transfer ended, so failures reach the caller instead of
// being indistinguishable from a transfer that never completed.
//
// Contract for implementers of this callback: it runs in an ISR. Set a flag,
// store the status, return. No blocking, no I2C calls, no floating point.
typedef void (*i2c_done_fn)(void *user_ctx, drv_status_t status);

// ------------------------------------------------------------------------------
// Transfer functions
// ------------------------------------------------------------------------------
// Blocking register read: addressed write of reg_addr, repeated START, then len
// bytes in. Returns DRV_ERR_TIMEOUT rather than spinning forever.
//
// Every blocking call takes a timeout. An unbounded `while (!flag);` hangs the
// firmware on a missing pull-up or an absent device -- and a hang while an ESC
// is armed is a safety failure, not an inconvenience.
typedef drv_status_t (*i2c_read_fn)(void *bus,
                                    uint8_t dev_addr,
                                    uint8_t reg_addr,
                                    uint8_t *data,
                                    uint16_t len,
                                    uint32_t timeout_ms);

typedef drv_status_t (*i2c_write_fn)(void *bus,
                                     uint8_t dev_addr,
                                     uint8_t reg_addr,
                                     const uint8_t *data,
                                     uint16_t len,
                                     uint32_t timeout_ms);

// Non-blocking register read. Returns immediately after starting the transfer;
// `done` fires from interrupt context when it finishes.
//
// Note there is deliberately no separate "_IT" and "_DMA" entry point. Whether
// the implementation uses interrupts, DMA, or both is an implementation detail
// of the bus, and exposing it doubles the API for no caller benefit.
//
// `data` belongs to the transport until `done` fires. The caller must not read
// or write it in between.
typedef drv_status_t (*i2c_read_async_fn)(void *bus,
                                          uint8_t dev_addr,
                                          uint8_t reg_addr,
                                          uint8_t *data,
                                          uint16_t len,
                                          i2c_done_fn done,
                                          void *user_ctx);

// ------------------------------------------------------------------------------
// The interface a device driver is handed at init time
// ------------------------------------------------------------------------------
typedef struct
{
    void *bus;                      // Opaque to the device driver. Passed back
                                    // as the first argument of every call.
    i2c_read_fn read;
    i2c_write_fn write;
    i2c_read_async_fn read_async;   // May be NULL if this bus offers no async
                                    // path. Device drivers must check.
} i2c_transport_t;

#endif // I2C_TRANSPORT_H
