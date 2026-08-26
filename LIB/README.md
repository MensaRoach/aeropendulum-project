# LIB

Portable device drivers and shared abstractions. Code in this directory must compile against
**no STM32 peripheral headers** — `stm32f4xx_ll_i2c.h` and friends are physically unreachable here.
This is enforced by the build: `LIB/` targets link only `${BOARD_NAME}_cpu` (CPU/FPU flags), not
`${BOARD_NAME}_config` (which carries the LL header paths). Accidentally adding an STM32 include
to a file under `LIB/` is a build error, not a code-review comment.

## What belongs here vs. in `DRIVERS/`

| Belongs in `LIB/` | Belongs in `DRIVERS/<family>/` |
|---|---|
| Device driver (knows the *device*: MPU-6050, ESC protocol) | Platform driver (knows the *MCU peripheral*: I2C, USART, timer) |
| Code that must compile on any Cortex-M4 | Code that calls `LL_I2C_*`, `LL_USART_*`, etc. |
| Interface/type headers shared by both layers | Implementation files that include `stm32f4xx_ll_*.h` |
| Anything you'd want to run on a host-side unit test | Anything that would fail outside the MCU context |

The concrete seam is `LIB/drv_common/inc/i2c_transport.h`. A device driver (`LIB/`) calls through
that interface; a platform driver (`DRIVERS/stm32f4/`) implements it.

## Submodule-ready rule

Each library under `LIB/` is self-contained:

- Its own `inc/` and `src/`.
- Its own `CMakeLists.txt` that declares all dependencies explicitly.
- A `README.md` is encouraged.

Any `LIB/<device>/` could be extracted into a standalone git repository and added back as a
submodule without restructuring. Do not add cross-library `#include` paths except through declared
CMake dependencies.

## Contents

### `drv_common/`

Header-only INTERFACE library. No source files.

| File | Role |
|---|---|
| `inc/drv_status.h` | `drv_status_t` — the shared return type for every driver entry point. One value per distinct recovery action; `DRV_OK` / `DRV_ERR_STATE` / `DRV_ERR_NACK` / `DRV_ERR_TIMEOUT` / `DRV_ERR_BUS`. |
| `inc/i2c_transport.h` | The hardware-agnostic I2C interface. `i2c_bus_t` struct + `i2c_read` / `i2c_write` / `i2c_bus_recover` function pointers. No MCU headers, no LL, no includes beyond `drv_status.h`. |

### `mpu6050/`

STATIC library. Links `drv_common` (PUBLIC) and `${BOARD_NAME}_cpu` (PRIVATE, for MCU ABI only).

| File | Role |
|---|---|
| `inc/mpu6050.h` | MPU-6050 public API. Configuration, acquisition, raw-count conversion. All stubs. |
| `src/mpu6050.c` | Implementation — currently all stubs returning `DRV_ERR_STATE`. |

See [.docs/homework/homework_01.html](../.docs/homework/homework_01.html) Part C for the
implementation assignments. See [.docs/driver_development_plan.md](../.docs/driver_development_plan.md)
for milestone IDs `I0`–`I4`.

### `esc/` *(not yet created)*

Will hold the portable ESC protocol layer (Assignment 05 equivalent). Same rules apply.