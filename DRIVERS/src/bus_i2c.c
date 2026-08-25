#include "bus_i2c.h"

// ==============================================================================
// STM32F4 I2C bus driver -- SCAFFOLD ONLY
// ==============================================================================
// Every function below is unimplemented and returns DRV_ERR_STATE. That is the
// deliberate stub convention across DRIVERS/: a stub must never return DRV_OK,
// because a caller that checks the return value would see success and carry on
// against a device that was never configured.
//
// TODO tags refer to milestones in .docs/driver_development_plan.md.

// ------------------------------------------------------------------------------
// Static helpers
// ------------------------------------------------------------------------------
// Naming follows the repo convention: MODULE_PascalCase for static hardware
// helpers, snake_case for the public API.
//
// TODO(I1): recover the register read/write plumbing from git history --
//   git show 11c4068^:APPS/src/mpu6050_telemetry.c
// It is sensor-agnostic and worth porting rather than rewriting. Two defects to
// fix on the way in, though, or "refactor don't rewrite" just inherits them:
//
//   1. The old MPU6050_ReadRegs handles size == 1, then loops `while (size > 3)`,
//      then unconditionally reads three bytes. Passing size == 2 (or 0) writes
//      past the end of the caller's buffer. It never fired because the only call
//      site passed 14 -- a general-purpose bus gets called with 2 constantly, for
//      any 16-bit register pair. The F4 two-byte read also needs the POS bit set
//      before ADDR is cleared; it is a genuinely different sequence, not a
//      special case of the N >= 3 one.
//
//   2. Every wait in it is an unbounded `while (!flag);`. A missing pull-up or an
//      absent device hangs the firmware forever. Each one needs a deadline --
//      hence the timeout_ms parameter on every blocking entry point here.

// TODO(I1): a monotonic millisecond source for deadlines. SysTick is already
// running (LL_Init1msTick in SystemClock_Config). Decide whether to expose it as
// a helper here or take a timebase function pointer, and mind the wrap.
// static bool I2C_DeadlineExpired(uint32_t start_ms, uint32_t timeout_ms);

// TODO(I1): map the hardware error flags onto drv_status_t. AF -> DRV_ERR_NACK,
// BERR/ARLO/OVR -> DRV_ERR_BUS. Clearing them is part of the job; a latched error
// flag wedges the next transfer.
// static drv_status_t I2C_TranslateError(I2C_TypeDef *instance);

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------
drv_status_t i2c_bus_init(i2c_bus_t *bus, i2c_speed_t speed)
{
    (void)bus;
    (void)speed;

    // TODO(I1): validate the handle, run i2c_bus_recover(), apply `speed` if it
    // differs from what MX_I2C1_Init() programmed, then LL_I2C_Enable().
    // Remember LL_I2C_Init() left the peripheral disabled.
    //
    // On failure leave it disabled and `initialised` false -- see the contract in
    // the header.
    return DRV_ERR_STATE;
}

drv_status_t i2c_bus_recover(i2c_bus_t *bus)
{
    (void)bus;

    // TODO(I1): port the bit-banged recovery from git history. Sequence is:
    // reconfigure SCL/SDA as open-drain outputs, clock SCL until SDA is released,
    // issue a manual STOP, restore the pins to gpio_af.
    //
    // Worth improving on the original while porting: it clocked a fixed nine
    // times regardless. Checking whether SDA has actually gone high, and
    // reporting failure if it never does, turns a hopeful ritual into a test.
    return DRV_ERR_STATE;
}

// ------------------------------------------------------------------------------
// Blocking transfers
// ------------------------------------------------------------------------------
drv_status_t i2c_bus_read(void *bus, uint8_t dev_addr, uint8_t reg_addr,
                          uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    (void)timeout_ms;

    // TODO(I1): START, addr+W, reg_addr, repeated START, addr+R, then len bytes.
    // dev_addr arrives 7-bit and right-aligned -- shift it here, in one place.
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

    // TODO(I1): START, addr+W, reg_addr, len bytes, STOP.
    return DRV_ERR_STATE;
}

// ------------------------------------------------------------------------------
// Async transfer
// ------------------------------------------------------------------------------
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

    // TODO(I3): blocked on two things, in this order:
    //
    //   1. A measurement. Instrument i2c_bus_read() with DWT->CYCCNT or a GPIO
    //      toggle and find out what a 14-byte burst actually costs at 400 kHz. If
    //      it lands near the predicted ~10% of a 4 ms period, this whole path may
    //      not be worth building. Measure first, then decide.
    //
    //   2. I2C1_EV_IRQn and I2C1_ER_IRQn enabled in CubeMX. Without them there is
    //      nothing to drive the address phase, and on I2C v1 DMA cannot do it.
    //
    // Shape when it does get built: run START / addr+W / reg / repeated START /
    // addr+R from the event ISR, then arm DMA for the data phase and set the CR2
    // LAST bit so the peripheral NACKs the final byte itself.
    return DRV_ERR_STATE;
}

bool i2c_bus_is_busy(const i2c_bus_t *bus)
{
    (void)bus;
    return false;
}

// ------------------------------------------------------------------------------
// Binding
// ------------------------------------------------------------------------------
i2c_transport_t i2c_bus_transport(i2c_bus_t *bus)
{
    // No casts anywhere in here. The three entry points already take void* as
    // their first parameter precisely so they match the typedefs in
    // i2c_transport.h exactly. If a cast ever becomes necessary, the signatures
    // have drifted and the fix belongs in the signatures, not here.
    i2c_transport_t t;

    t.bus = bus;
    t.read = i2c_bus_read;
    t.write = i2c_bus_write;
    t.read_async = i2c_bus_read_async;

    return t;
}

// ------------------------------------------------------------------------------
// Interrupt entry points
// ------------------------------------------------------------------------------
// These are called from USER CODE blocks in Core/Src/stm32f4xx_it.c. The driver
// does not define the IRQ handlers itself, so CubeMX regeneration cannot clobber
// the wiring.
//
// Whatever ends up in here: set a flag, store a status, fire the callback, get
// out. No blocking, no floating point.

void i2c_bus_on_event_irq(i2c_bus_t *bus)
{
    (void)bus;
    // TODO(I3): address-phase state machine. Not reachable until I2C1_EV_IRQn is
    // enabled in CubeMX.
}

void i2c_bus_on_error_irq(i2c_bus_t *bus)
{
    (void)bus;
    // TODO(I3): translate and clear the error flags, abort the transfer, clear
    // `busy`, fire done_cb with the failure status. A transfer that dies without
    // firing its callback strands the caller waiting forever.
}

void i2c_bus_on_dma_rx_irq(i2c_bus_t *bus)
{
    (void)bus;
    // TODO(I3): data phase complete -- STOP, clear `busy`, fire done_cb.
}

void i2c_bus_on_dma_tx_irq(i2c_bus_t *bus)
{
    (void)bus;
    // TODO(I3): transmit path, only if async writes turn out to be needed.
}
