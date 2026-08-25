# CLAUDE.md

Guidance for Claude Code agents working in this repository.

## What this is

A hobby embedded project building an **aeropendulum** (propeller-driven pendulum, controlled attitude).
Firmware runs on a **NUCLEO-F401RE** (STM32F401RET6, Cortex-M4F @ 84 MHz) using **STM32 LL drivers**
(not HAL). Host-side Python tools in `tools/` consume the board's serial telemetry — though nothing
currently emits any; see below.

Current state: the MPU6050 firmware driver was **deliberately deleted** and is being rewritten from
scratch as a learning exercise, alongside an ESC driver that does not exist yet. `DRIVERS/` now holds
a **scaffold for that rewrite — interfaces and contracts only, no implementation**: the headers are
complete and documented, and every function body is an empty stub returning `DRV_ERR_STATE`. It
compiles and links; it does nothing. The implementation work is set out as guided assignments in
[.docs/homework/](.docs/homework/). See [DRIVERS/README.md](DRIVERS/README.md) for the layering rule.

**Keep `DRIVERS/` free of `TODO` comments and teaching notes.** The source is written as a published
library; the tasks, hints and rationale live in `.docs/homework/` instead.

What actually runs on the board is USART output, COBS framing, and two trivial demo apps. The
host-side Python tools in `tools/` were kept and still expect the old frame format — see
[Telemetry protocol](#telemetry-protocol). No sensor reads, no motor drive and no control loop exist
yet — that is the work still ahead.

## Build & flash

Toolchain is **STM32CubeCLT** (arm-none-eabi-gcc + Ninja). Build directories live in `.build/<presetName>/`.

```bash
cmake --preset NUCLEO-F401RE-Debug-Local && cmake --build .build/NUCLEO-F401RE-Debug-Local
```

`<board>-Debug-Local` presets are **not** in `CMakePresets.json` — they live in `CMakeUserPresets.json`, which is
gitignored because it is machine-specific. It inherits the base board preset (e.g. `NUCLEO-F401RE-Debug`) and prepends the local STM32CubeCLT
`bin` directories to `PATH`; see [README.md](README.md) for the template. On this machine CubeCLT is
at `C:/ST/STM32CubeCLT_1.21.0`, and `NUCLEO-F401RE-Release-Local` exists alongside it. On a machine without that
file, either write one or use the shared presets with the toolchain already on `PATH`:

```bash
cmake --preset NUCLEO-F401RE-Debug && cmake --build .build/NUCLEO-F401RE-Debug
```

`cmake`, `ninja`, and `arm-none-eabi-gcc` are **not** on the system `PATH` here — they come from the
preset's environment block. Invoking `cmake` directly from a shell needs the full path
(`C:/ST/STM32CubeCLT_1.21.0/CMake/bin/cmake.exe`).

Output is `.build/<preset>/NUCLEO-F401RE.elf` plus `NUCLEO-F401RE.map` (placed next to the `.elf`). Flashing/debugging goes through
the VS Code `Debug / Flash NUCLEO-F401RE (Cortex-Debug)` launch config (`servertype: stlink`), which runs the `Build` task first.

There is **no test suite** and no linter configured. Verification is: it compiles, and — once a sensor
app exists again — the telemetry stream decodes on the host.

## Layout

| Path | Role |
|---|---|
| `APPS/` | Application logic and wiring. Compiles to static lib `apps`, linked into the board executable. |
| `DRIVERS/` | Peripheral and device drivers (`inc/` + `src/`). Compiles to static lib `drivers`. Currently a scaffold — see [DRIVERS/README.md](DRIVERS/README.md). |
| `BOARDS/NUCLEO-F401RE/` | STM32CubeIDE-generated board support (`Core/`, `Drivers/`, `.ioc`, linker scripts, `pin_definitions.h`, `board.cmake`). Regenerate via CubeMX; don't hand-edit. |
| `BOARDS/<board>/board.cmake` | Board manifest defining MCU family, device, CPU profile, driver mode, debug target. |
| `.cmake/` | `gcc-arm-none-eabi.cmake` (toolchain), `cortex_profiles.cmake` (CPU flag mappings), `stm32.cmake` (`add_stm32_board()` wrapper and board discovery). |
| `tools/` | Host-side Python: telemetry reader, 3D pose visualizer, IMU-driven synth. |
| `.docs/` | Component datasheets and reference PDFs, plus the project's own writeups — see [Documentation map](#documentation-map). |
| `.build/` | Build output (gitignored). |

## Documentation map

| Document | Status | Notes |
|---|---|---|
| [.docs/feasibility_analysis.md](.docs/feasibility_analysis.md) | **Authoritative** on hardware, sizing, powertrain and safety | Dated, with a firmware baseline. Its headline finding is a battery-voltage decision. **Spoiler source** — see below. |
| [.docs/hardware_feasibility.md](.docs/hardware_feasibility.md) | **Superseded** | An earlier, coarser pass. Kept for history; carries a supersession banner. Do not cite it. |
| [.docs/driver_development_plan.md](.docs/driver_development_plan.md) | Active | The process document for the ESC + IMU rewrite — the *why* and the ordering. Milestone IDs `I0`–`I4`, `E0`–`E4`. |
| [.docs/homework/](.docs/homework/) | Active | Step-by-step implementation assignments — the *how*. Assignment 01 covers the I2C bus driver and the MPU-6050 library. |
| [.docs/driver_development_log.md](.docs/driver_development_log.md) | Active | Decisions log. One entry per non-obvious choice, with reasoning. |
| [.docs/imu_data_processing.md](.docs/imu_data_processing.md) | Reference, **describes deleted firmware** | The math is still correct and the host tools still implement it. The firmware it describes no longer exists. **Spoiler source.** |
| [.docs/cmake_multi_board_plan.md](.docs/cmake_multi_board_plan.md) | **Implemented** | Historical implementation brief. Kept for the reasoning in §4 (rejected alternatives) and §7. |
| [DRIVERS/README.md](DRIVERS/README.md) | Active | Layering rule and driver conventions. |

### Spoilers

The driver rewrite is a **deliberate learning exercise**. `driver_development_plan.md` asks for several
answers to be derived from primary sources rather than looked up, and names the documents that give
those answers away: sections 5–7 of `feasibility_analysis.md`, all of `hardware_feasibility.md`, and
`imu_data_processing.md`.

If you are an agent working here: don't volunteer register addresses, scale factors, the device
address, timer clock frequencies, a timer/pin pairing, a target PWM frequency, or the ESC arming
sequence unless asked for them directly. Reading these files to answer a question about the *repo* is
fine; reproducing their answers into a reply is what defeats the exercise.

## How the build wires together

`CMakeLists.txt` discovers available boards via `stm32_list_boards(AVAILABLE_BOARDS)` and calls
`add_stm32_board(${BOARD_NAME})` (e.g. `add_stm32_board(NUCLEO-F401RE)`) from [.cmake/stm32.cmake](.cmake/stm32.cmake), which:

- loads `BOARDS/<board>/board.cmake` to read the board manifest (`MCU_FAMILY`, `DEVICE`, `CPU_PROFILE`, `DRIVER_MODE`, `DEBUG_DEVICE`);
- resolves CPU flags via `cortex_flags(${CPU_PROFILE})` from [.cmake/cortex_profiles.cmake](.cmake/cortex_profiles.cmake);
- creates an INTERFACE library **`${BOARD_NAME}_config`** carrying include paths, CPU compiler and linker flags, and the defines (e.g. `USE_FULL_LL_DRIVER` + `STM32F401xE`);
- globs `Core/Src/*.c`, `Drivers/<FAMILY_DIR>_HAL_Driver/Src/*.c`, and `Core/Startup/*.s` with `CONFIGURE_DEPENDS` into the executable target `${BOARD_NAME}`;
- finds the linker script (`*FLASH.ld` or manifest override) and sets per-board link options including `-Wl,-Map=$<TARGET_FILE_DIR:${BOARD}>/${BOARD}.map`.

`DRIVERS/CMakeLists.txt` and `APPS/CMakeLists.txt` each glob `src/*.c` with `CONFIGURE_DEPENDS` into
the static libs `drivers` and `apps`, and link both into the board executable. The root
`CMakeLists.txt` adds `DRIVERS` **before** `APPS`, because `apps` links `drivers`.

Any new library target must link `${BOARD_NAME}_config` to inherit the MCU flags and include paths.

### Where code goes

| Kind of code | Directory |
|---|---|
| Peripheral drivers (I2C, USART, timers) and device drivers (IMU, ESC) | `DRIVERS/` |
| Apps, wiring, control logic — anything that picks a concrete peripheral | `APPS/` |

`drivers` deliberately has **no include path to `BOARDS/${BOARD_NAME}`**, so driver sources cannot
reach `pin_definitions.h`. Choosing which peripheral a device sits on is the application's job; the
missing include path is what makes that rule enforced rather than merely documented.

## Application model

Applications are selected **at compile time**, not at runtime. Exactly one `#define` is uncommented in
[APPS/inc/app_selector.h](APPS/inc/app_selector.h):

```c
// #define APP_HELLO_USART
#define APP_BLINKY
```

[APPS/src/main_app.c](APPS/src/main_app.c) `#ifdef`-dispatches into `<app>_init()` / `<app>_loop()`.
Generated `main.c` calls `app_main()` inside `/* USER CODE BEGIN 2 */`, which inits then loops forever.

To add an app: create `APPS/src/<name>.c` + `APPS/inc/<name>.h` exposing `<name>_init(void)` /
`<name>_loop(void)`, add an `APP_<NAME>` define to `app_selector.h`, and add the two `#ifdef` blocks
in `main_app.c`.

Every `APPS/src/*.c` is compiled into `libapps.a` regardless of which app is selected; the `#ifdef`s
only decide what gets *called*, and the linker's `--gc-sections` drops the rest. So a broken app
still breaks the build even when it is not the selected one.

The two implemented apps are `APP_HELLO_USART` (prints `Hello` over USART2) and `APP_BLINKY`
(toggles LD2 at 1 Hz — the quickest "is the board alive" check, and currently selected). A third,
`APP_MPU6050_TELEMETRY`, was removed along with its driver; see the note at the top of this file.

## Telemetry protocol

No firmware currently emits telemetry — the app that did was deleted. This section records the format
the **host tools still expect**, so that a replacement driver either matches it or `tools/` gets
updated alongside it.

The historical payload was a packed 14-byte struct (7 × `int16_t` — ax, ay, az, temp, gx, gy, gz,
little-endian), COBS-encoded and sent over USART2 followed by a `0x00` frame delimiter at roughly 100 Hz.

Wire settings are **115200 8E1** — CubeMX configures USART2 as `DATAWIDTH_9B` + `PARITY_EVEN`, which
is 8 data bits plus a parity bit on the wire. Host readers must open the port with `PARITY_EVEN`,
or every frame fails to decode.

The firmware COBS implementation is [APPS/src/cobs.c](APPS/src/cobs.c) (encode + decode, no dependencies)
and was **kept** — it is sensor-agnostic, and `cobs_decode()` is currently unused. The host uses the
`cobs` PyPI package.
[.docs/imu_data_processing.md](.docs/imu_data_processing.md) documents the full chain including the
complementary filter — read it before touching orientation math.

## Hardware map

Peripheral instances are aliased in [BOARDS/NUCLEO-F401RE/pin_definitions.h](BOARDS/NUCLEO-F401RE/pin_definitions.h);
**always use those aliases in `APPS/`, never the raw `USART2` / `I2C1` handles.**

- `TELEMETRY_USART` = USART2 on PA2/PA3 (routed to the ST-LINK virtual COM port)
- `STATUS_LED_PORT` / `STATUS_LED_PIN` = LD2 on PA5

- `IMU_I2C` = I2C1 on PB6/PB7 (AF4, open-drain, 100 kHz), with `IMU_I2C_PORT` / `IMU_I2C_SCL_PIN` /
  `IMU_I2C_SDA_PIN` / `IMU_I2C_AF` alongside it. The GPIO aliases exist for **bus recovery only** —
  normal traffic goes through the peripheral.

**`MX_I2C1_Init()` leaves the peripheral disabled.** `LL_I2C_Init()` ends with `LL_I2C_Disable()` and
never re-enables, so something has to switch I2C1 on before use. That is the seam this project uses to
answer "who owns the peripheral": **CubeMX owns configuration, the driver owns enable and recovery.**
The same question is worth re-checking for USART2 rather than assumed to match.

The `.ioc` only declares USART2, I2C1 and SWD, so **`MX_GPIO_Init()` configures every other pin —
PA5 included — as analog**. An app that wants a GPIO must call `LL_GPIO_Init()` on it first; see
`blinky_init()`. The port clocks are already enabled for GPIOA/B/C/D/H, so only the pin mode needs
setting.

An I2C slave can be left holding SDA low across an MCU reset, wedging the bus before the peripheral is
even enabled; the escape is a **bit-banged bus recovery**. The deleted driver did this and the
deleted driver did this, and `i2c_bus_recover()` is declared for it in `DRIVERS/` but **not yet
implemented**.

> The old working I2C code is still in git history. **Do not surface it to whoever is working
> through the assignments** — deriving the flag sequences from the RM0368 flowcharts is the exercise,
> and handing over the answer removes the transferable part of it. Use it to check a claim if you
> need to; don't paste it into a reply.

**DMA is configured but unused.** The `.ioc` maps DMA1 Stream0 Ch1 → I2C1_RX and Stream6 Ch1 → I2C1_TX,
and `MX_DMA_Init()` enables both NVIC lines — but `DMA1_Stream0_IRQHandler` / `DMA1_Stream6_IRQHandler`
in `stm32f4xx_it.c` have empty `USER CODE` blocks, so nothing responds. Two things to know before
building on it:

- **`I2C1_EV_IRQn` and `I2C1_ER_IRQn` are not enabled.** The F401 has the **I2C v1** peripheral, where
  DMA moves the *data* phase only — START, address, register pointer and repeated START must be driven
  by the CPU, which for a non-blocking driver means the event interrupt. Enabling those two NVIC lines
  in CubeMX is a prerequisite for any async I2C path. Multi-byte DMA reception also needs the CR2 LAST
  bit (`LL_I2C_EnableLastDMA`).
- **Most "STM32 I2C DMA" examples online target I2C v2** (F0/F3/F7/L4/G4), where `NBYTES` and `AUTOEND`
  make this straightforward. None of that transfers to this chip.

Everything that currently executes uses blocking polled LL (`while (!LL_..._IsActiveFlag_...())`).
Driver interrupt hooks are named `<module>_on_*_irq()` and are meant to be called from the `USER CODE`
blocks in `stm32f4xx_it.c`, so drivers never define IRQ handlers themselves and CubeMX regeneration
cannot clobber the wiring.

## Host tools

Deps are declared in [tools/requirements.txt](tools/requirements.txt) (pyserial, cobs, numpy,
pygame-ce, PyOpenGL, sounddevice). The `.venv/` in this checkout is Windows-specific and untracked;
on another machine, create a fresh one and `pip install -r tools/requirements.txt`.

```bash
python tools/read_telemetry.py -p COM8
```

The port argument is platform-specific: `COM*` on Windows, `/dev/cu.usbmodem*` on macOS (the `cu.`
device — `tty.` blocks on carrier detect), `/dev/ttyACM*` on Linux.

- `tools/read_telemetry.py` — prints decoded frames; simplest check that the board is alive.
- `tools/visualize_imu.py` — real-time 3D pose via OpenGL, complementary filter.
- `tools/imu_music/main.py` — IMU-driven synth + visualizer. Uses **flat imports** (`from imu_reader import IMU`),
  so it must be run from inside `tools/imu_music/`.

`out.txt` in the repo root is a captured sample session, useful as reference for expected output.

## Conventions

1. **LL drivers only.** Use `LL_*` functions; do not introduce HAL. `USE_FULL_LL_DRIVER` is set globally.
2. **Never use the CubeMX CMake generator.** This project deliberately wraps the CubeIDE project layout
   with its own CMake; regenerating with CubeMX's CMake output will break it.
3. **Keep application code out of generated files.** The only intended edit to `main.c` is the existing
   `app_main()` call inside a `USER CODE` block. Anything else goes in `APPS/`.
4. Braces on their own line, 4-space indent, `snake_case` for public functions, `MODULE_PascalCase`
   for static hardware helpers (e.g. `MPU6050_ReadRegs`). Headers use `#ifndef`/`#define` guards.
   `DRIVERS/` demonstrates both.
5. **Driver conventions** (established in `DRIVERS/`, see its README for the reasoning):
   - Every driver entry point returns `drv_status_t`, never `bool` — a caller that only learns "it
     failed" cannot choose between retrying, running bus recovery, and disarming.
   - Every blocking call takes a timeout. No unbounded `while (!flag);` anywhere.
   - I2C device addresses are **7-bit, right-aligned, never pre-shifted**.
   - Stubs return `DRV_ERR_STATE`, never `DRV_OK`. A stub that reports success is worse than one that
     fails, because the caller carries on against a device that was never configured.
   - Device drivers depend on the abstract transport header; platform drivers implement it. Neither
     includes the other.

## Adding a board

1. Generate the board in STM32CubeIDE with **LL drivers**. Copy the export to `BOARDS/<board>/`. (Never the CubeMX CMake generator — see [Conventions](#conventions).)
2. Write `BOARDS/<board>/board.cmake` and `BOARDS/<board>/pin_definitions.h`.
3. Add the hidden preset, two concrete presets, and two build presets to `CMakePresets.json`.
4. Add a `.vscode/launch.json` configuration with that board's `device`.
5. Build with `cmake --preset <board>-Debug && cmake --build .build/<board>-Debug`.

## Gotchas

- **`STM32F401RETX_FLASH.ld` must stay tracked.** `.cmake/stm32.cmake` globs `*FLASH.ld` and hard-fails
  with `FATAL_ERROR` when it finds none, so a clone without it cannot configure at all. `.gitignore`
  used to exclude it via a `*FLASH.ld` rule; that rule has been removed. Don't reintroduce it, and
  don't "fix" a missing script by pointing the build at `STM32F401RETX_RAM.ld` — that links the image
  to run from RAM and it will not survive a power cycle.
- **`.mxproject` is deliberately untracked.** It is CubeMX's manifest of what it generated last time,
  and it stores paths with the host's separator — so tracking it means a whole-file diff every time
  regeneration happens on a different OS. Each machine keeps its own local copy instead.
  The cost: after **removing** a peripheral from the `.ioc`, a machine whose manifest is stale (or
  absent, on a fresh clone) will not delete the now-orphaned `Core/Src/<periph>.c`. Since
  `.cmake/stm32.cmake` globs `Core/Src/*.c`, an orphan keeps compiling into the firmware silently.
  After any peripheral removal, check `git status` for generated files that should have disappeared.
- `.cmake/` and `.docs/` are dot-prefixed on purpose — some tooling and shell globs skip them.
