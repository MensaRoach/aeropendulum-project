#ifndef BUS_I2C_H
#define BUS_I2C_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32f4xx_ll_i2c.h"
#include "stm32f4xx_ll_gpio.h"

#include "drv_status.h"
#include "i2c_transport.h"

// ==============================================================================
// STM32F4 I2C bus driver (platform layer)
// ==============================================================================
// This is the MCU-specific half of the I2C stack. Device drivers must NOT include
// this header -- they take an i2c_transport_t instead. Only board/app wiring code
// includes this file.
//
// ------------------------------------------------------------------------------
// Ownership contract  (Phase 0 question, answered)
// ------------------------------------------------------------------------------
// The split is not a matter of taste here -- the toolchain already drew the line:
//
//   CubeMX owns CONFIGURATION.  MX_GPIO_Init() sets PB6/PB7 to AF4 open-drain and
//                               MX_I2C1_Init() programs CR2/CCR/TRISE/OAR.
//
//   This driver owns ENABLE and RECOVERY.  LL_I2C_Init() ends with
//                               LL_I2C_Disable() and never re-enables, so the
//                               peripheral is left off after MX_I2C1_Init()
//                               returns. Somebody has to switch it on; that
//                               somebody is i2c_bus_init().
//
// The bus is a SHARED resource with a busy flag, not something a device owns.
// Devices do not init or de-init it. One i2c_bus_t is created once by app wiring;
// every device on that bus gets a transport bound to the same instance. Tearing
// the peripheral down and back up between devices would be both expensive and
// racy, and it is not how a shared bus works.
//
// ------------------------------------------------------------------------------
// Bus recovery
// ------------------------------------------------------------------------------
// A slave can be left mid-byte holding SDA low across an MCU reset, which wedges
// the bus before the peripheral is even enabled. The escape is to bit-bang SCL
// until the slave releases SDA, then issue a manual STOP, then hand the pins back
// to the peripheral. That is the only reason this struct needs to know the pins.
//
// ------------------------------------------------------------------------------
// Prerequisite for the async path
// ------------------------------------------------------------------------------
// i2c_bus_read_async() cannot work with the current CubeMX configuration.
//
// The F401 carries the I2C v1 peripheral. DMA moves the DATA phase only -- START,
// address+W, register pointer, repeated START and address+R must all be driven by
// the CPU, which for a non-blocking implementation means the event interrupt. The
// .ioc currently enables DMA1_Stream0_IRQn and DMA1_Stream6_IRQn but NOT
// I2C1_EV_IRQn or I2C1_ER_IRQn, so there is nothing to drive the address phase.
//
// Enabling those two NVIC lines in CubeMX is a prerequisite, not part of this
// scaffold. Also note that multi-byte DMA reception needs the CR2 LAST bit
// (LL_I2C_EnableLastDMA) so the peripheral NACKs the final byte itself.
//
// Worth internalising before searching for examples: nearly every "STM32 I2C DMA"
// article online targets the I2C v2 peripheral (F0/F3/F7/L4/G4), where NBYTES and
// AUTOEND make this straightforward. None of it transfers to this chip.

// ------------------------------------------------------------------------------
// Configuration
// ------------------------------------------------------------------------------
typedef enum
{
    I2C_SPEED_STANDARD_100K,  // What the .ioc currently programs.
    I2C_SPEED_FAST_400K       // Supported by the MPU6050. A 14-byte burst read
                              // costs roughly 1.56 ms at 100 kHz and 0.39 ms at
                              // 400 kHz -- 39% vs 10% of a 4 ms control period.
                              // Changing bus speed is far cheaper than building
                              // an async path. Measure before choosing.
} i2c_speed_t;

// ------------------------------------------------------------------------------
// Bus handle
// ------------------------------------------------------------------------------
// The struct is public so that callers can allocate one, e.g.
//
//     static i2c_bus_t s_imu_bus;
//
// An opaque type with no factory function cannot be instantiated at all.
//
// Fields are split into two groups. Fill in the first group before calling
// i2c_bus_init(); never touch the second group from application code.
typedef struct
{
    // --- set by the caller before i2c_bus_init() ---------------------------
    I2C_TypeDef *instance;      // e.g. IMU_I2C from pin_definitions.h
    GPIO_TypeDef *gpio_port;    // Port carrying SCL and SDA (recovery only)
    uint32_t scl_pin;           // LL_GPIO_PIN_x
    uint32_t sda_pin;           // LL_GPIO_PIN_x
    uint32_t gpio_af;           // LL_GPIO_AF_x to restore after recovery

    // --- private: owned by bus_i2c.c ---------------------------------------
    i2c_done_fn done_cb;
    void *done_ctx;
    uint8_t *rx_ptr;
    uint16_t rx_len;
    uint8_t pending_dev_addr;
    uint8_t pending_reg_addr;
    volatile bool busy;
    volatile drv_status_t last_status;
    bool initialised;
} i2c_bus_t;

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------
// Enables the peripheral, having first run recovery. Assumes MX_I2C1_Init() has
// already configured it.
//
// init() contract (Phase 0 question, answered): on failure the bus is left
// DISABLED and `initialised` stays false. Every transfer function returns
// DRV_ERR_STATE while that is the case -- calling read/write after a failed init
// is defined behaviour, not undefined.
drv_status_t i2c_bus_init(i2c_bus_t *bus, i2c_speed_t speed);

// Bit-bangs the bus free of a stuck slave and restores the pins to AF mode.
// Safe to call at any time; used by i2c_bus_init() and after DRV_ERR_TIMEOUT.
drv_status_t i2c_bus_recover(i2c_bus_t *bus);

// ------------------------------------------------------------------------------
// Transfers  (signatures deliberately match the typedefs in i2c_transport.h)
// ------------------------------------------------------------------------------
drv_status_t i2c_bus_read(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                          uint8_t *data, uint16_t len, uint32_t timeout_ms);

drv_status_t i2c_bus_write(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                           const uint8_t *data, uint16_t len, uint32_t timeout_ms);

drv_status_t i2c_bus_read_async(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len,
                                i2c_done_fn done, void *user_ctx);

bool i2c_bus_is_busy(const i2c_bus_t *bus);

// ------------------------------------------------------------------------------
// Binding
// ------------------------------------------------------------------------------
// Produces the device-facing view of this bus. App wiring calls this once per
// device and hands the result to that device's init.
i2c_transport_t i2c_bus_transport(i2c_bus_t *bus);

// ------------------------------------------------------------------------------
// Interrupt entry points
// ------------------------------------------------------------------------------
// Call these from the matching USER CODE blocks in Core/Src/stm32f4xx_it.c. That
// is the only sanctioned edit to a generated file -- the driver never defines the
// IRQ handlers itself, so CubeMX regeneration cannot clobber it.
void i2c_bus_on_event_irq(i2c_bus_t *bus);   // I2C1_EV_IRQHandler   (not yet enabled)
void i2c_bus_on_error_irq(i2c_bus_t *bus);   // I2C1_ER_IRQHandler   (not yet enabled)
void i2c_bus_on_dma_rx_irq(i2c_bus_t *bus);  // DMA1_Stream0_IRQHandler
void i2c_bus_on_dma_tx_irq(i2c_bus_t *bus);  // DMA1_Stream6_IRQHandler

#endif // BUS_I2C_H
