# DRIVERS

Platform drivers for the STM32F4 family, implementing the hardware-agnostic interfaces
declared in [`LIB/drv_common/`](../LIB/drv_common/).

Scaffolding for the from-scratch driver rewrite described in
[.docs/driver_development_plan.md](../.docs/driver_development_plan.md). The contracts encoded here
are the answers to that plan's Phase 0, reasoned through in
[.docs/driver_development_log.md](../.docs/driver_development_log.md).

**Nothing here is implemented.** The headers are complete and are the specification; every function
body is an empty stub returning `DRV_ERR_STATE`. The implementation work is set out task by task in
[.docs/homework/](../.docs/homework/) — start with
[Assignment 01](../.docs/homework/homework_01.html).

Source files carry no `TODO` comments or teaching notes by design: they are written as a published
library, and the guidance lives in the assignments instead.

## Layering

```
               APPS/  (wiring: picks the peripheral, owns the instances)
                 |
      +----------+--------------------+
      |                               |
 DRIVERS/stm32f4/            LIB/mpu6050/         <- two separate libraries
  bus_i2c.h                   mpu6050.h
  bus_uart.h                      |
      |                    i2c_transport.h         <- the seam (in LIB/drv_common/)
      |                           ^
      +------- implements --------+
```

Three rules, and everything else follows from them:

> **Device drivers (`LIB/`) depend on `i2c_transport.h`. Platform drivers (`DRIVERS/stm32f4/`)
> implement it. Neither includes the other.**

> **`DRIVERS/stm32f4/` is keyed per MCU family, not per board.** Both the STM32F401 and the
> STM32F407 carry the I2C v1 peripheral with identical register-level sequences. One implementation
> compiles and runs on both. `DRIVERS/CMakeLists.txt` selects the subdirectory from `MCU_FAMILY`.

> **`LIB/` targets never link `${BOARD_NAME}_config`.** They link only `${BOARD_NAME}_cpu`
> (CPU/FPU flags). This makes it a *build error* — not a code-review comment — to `#include` an
> STM32 peripheral header from inside a device driver.

So `mpu6050.c` contains no `stm32` include and no knowledge of DMA, and `bus_i2c.c`
contains no knowledge of accelerometers. Swapping the MPU6050 onto a bit-banged bus, or
onto a different MCU, touches one file. Porting to a new STM32F4 board requires no changes
to either library — only `APPS/` changes to wire up the right peripheral instance.

## Files

### `LIB/drv_common/` — shared abstractions

| File | Role |
|---|---|
| `inc/drv_status.h` | Shared `drv_status_t`. Every entry point returns one. |
| `inc/i2c_transport.h` | The hardware-agnostic bus interface. No MCU, no LL, no includes beyond `drv_status.h`. |

### `LIB/mpu6050/` — portable device driver

| File | Role |
|---|---|
| `inc/mpu6050.h` | MPU-6050 API. Register map and scaling only. No STM32 includes. |
| `src/mpu6050.c` | Implementation stub. Depends on `drv_common`; knows nothing about STM32. |

### `DRIVERS/stm32f4/` — STM32F4 platform drivers

| File | Role |
|---|---|
| `inc/bus_i2c.h` / `src/bus_i2c.c` | STM32F4 I2C. Owns enable + recovery; CubeMX owns configuration. |
| `inc/bus_uart.h` / `src/bus_uart.c` | STM32F4 USART. Not on the plan's critical path — see the scope note in the header. |

## Conventions

- `snake_case` for the public API, `MODULE_PascalCase` for static hardware helpers,
  matching [CLAUDE.md](../CLAUDE.md).
- **Stubs never return `DRV_OK`.** A stub that reports success is worse than one that
  fails: the caller carries on against a device that was never configured.
- **Every blocking call takes a timeout.** No unbounded `while (!flag);` anywhere. A hang
  with an ESC armed is a safety failure, not an inconvenience.
- **`dev_addr` is 7-bit, right-aligned, never pre-shifted.** Fixed once in
  `i2c_transport.h` and repeated in every doc comment, because mixing 7-bit and 8-bit
  addresses presents as total silence that looks exactly like a wiring fault.
- **No register values in headers.** They live in the `.c` that uses them, and they come
  from the InvenSense register map document — which is plan item I0 and is not yet in
  `.docs/IMU/`.

## Known prerequisites

- **`i2c_bus_read_async()` cannot work yet.** `I2C1_EV_IRQn` / `I2C1_ER_IRQn` are not
  enabled in the `.ioc`, and on the F401/F407 I2C v1 peripheral DMA moves the data phase
  only — the address phase needs the event interrupt. Enabling them is a CubeMX change.
- **Measure before building the async path.** A 14-byte burst read costs roughly 1.56 ms
  at 100 kHz and 0.39 ms at 400 kHz, against a 4 ms budget at 250 Hz. The bus speed knob
  is one CubeMX field; the async path is a state machine. Get the number first.