# DRIVERS

Scaffolding for the from-scratch driver rewrite described in
[.docs/driver_development_plan.md](../.docs/driver_development_plan.md). The contracts encoded here
are the answers to that plan's Phase 0, reasoned through in
[.docs/driver_development_log.md](../.docs/driver_development_log.md).

**Nothing here is implemented.** Every function returns `DRV_ERR_STATE` and carries a
`TODO(<milestone>)` pointing at the plan item that fills it in.

## Layering

```
                 APPS/  (wiring: picks the peripheral, owns the instances)
                   |
        +----------+----------+
        |                     |
   bus_i2c.h             mpu6050.h          <-- apps include both
        |                     |
        |              i2c_transport.h      <-- the seam
        |                     ^
        +---- implements -----+
```

One rule, and everything else follows from it:

> **Device drivers depend on `i2c_transport.h`. Platform drivers implement it.
> Neither includes the other.**

So `mpu6050.c` contains no `stm32` include and no knowledge of DMA, and `bus_i2c.c`
contains no knowledge of accelerometers. Swapping the MPU6050 onto a bit-banged bus, or
onto a different MCU, touches one file.

## Files

| File | Role |
|---|---|
| `inc/drv_status.h` | Shared `drv_status_t`. Every entry point returns one. |
| `inc/i2c_transport.h` | The hardware-agnostic bus interface. No MCU, no LL, no includes beyond `drv_status.h`. |
| `inc/bus_i2c.h` / `src/bus_i2c.c` | STM32F4 I2C. Owns enable + recovery; CubeMX owns configuration. |
| `inc/bus_uart.h` / `src/bus_uart.c` | STM32F4 USART. Not on the plan's critical path — see the scope note in the header. |
| `inc/mpu6050.h` / `src/mpu6050.c` | MPU6050. Register map and scaling only. |

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
  enabled in the `.ioc`, and on the F401's I2C v1 peripheral DMA moves the data phase
  only — the address phase needs the event interrupt. Enabling them is a CubeMX change.
- **Measure before building the async path.** A 14-byte burst read costs roughly 1.56 ms
  at 100 kHz and 0.39 ms at 400 kHz, against a 4 ms budget at 250 Hz. The bus speed knob
  is one CubeMX field; the async path is a state machine. Get the number first.
