# CLAUDE.md

Guidance for Claude Code agents working in this repository.

## What this is

A hobby embedded project building an **aeropendulum** (propeller-driven pendulum, controlled attitude).
Firmware runs on a **NUCLEO-F401RE** (STM32F401RET6, Cortex-M4F @ 84 MHz) using **STM32 LL drivers**
(not HAL). Host-side Python tools consume the board's serial telemetry.

Current state: the MPU6050 firmware driver has been **deliberately deleted**. It is being rewritten
from scratch as a learning exercise, alongside an ESC driver that does not exist yet. What remains on
the board is USART output, COBS framing, and two trivial demo apps. The host-side Python tools in
`tools/` were kept and still expect the old frame format — see [Telemetry protocol](#telemetry-protocol).
No motor drive and no control loop exist yet — that is the work still ahead.

## Build & flash

Toolchain is **STM32CubeCLT** (arm-none-eabi-gcc + Ninja). Build directories live in `.build/<presetName>/`.

```bash
cmake --preset Debug-Local && cmake --build .build/Debug-Local
```

`Debug-Local` is **not** in `CMakePresets.json` — it lives in `CMakeUserPresets.json`, which is
gitignored because it is machine-specific. It inherits `Debug` and prepends the local STM32CubeCLT
`bin` directories to `PATH`; see [README.md](README.md) for the template. On this machine CubeCLT is
at `C:/ST/STM32CubeCLT_1.21.0`, and `Release-Local` exists alongside it. On a machine without that
file, either write one or use the shared presets with the toolchain already on `PATH`:

```bash
cmake --preset Debug && cmake --build .build/Debug
```

`cmake`, `ninja`, and `arm-none-eabi-gcc` are **not** on the system `PATH` here — they come from the
preset's environment block. Invoking `cmake` directly from a shell needs the full path
(`C:/ST/STM32CubeCLT_1.21.0/CMake/bin/cmake.exe`).

Output is `.build/<preset>/NUCLEO-F401RE.elf` plus `Aeropendulum.map`. Flashing/debugging goes through
the VS Code `Debug / Flash (Cortex-Debug)` launch config (`servertype: stlink`), which runs the `Build` task first.

There is **no test suite** and no linter configured. Verification is: it compiles, and — once a sensor
app exists again — the telemetry stream decodes on the host.

## Layout

| Path | Role |
|---|---|
| `APPS/` | All custom logic. Compiles to static lib `apps`, linked into the board executable. **Write code here.** |
| `BOARDS/NUCLEO-F401RE/` | STM32CubeIDE-generated board support (`Core/`, `Drivers/`, `.ioc`, linker scripts). Regenerate via CubeMX; don't hand-edit. |
| `.cmake/` | `gcc-arm-none-eabi.cmake` (toolchain) + `stm32.cmake` (the `add_stm32_board()` wrapper). |
| `tools/` | Host-side Python: telemetry reader, 3D pose visualizer, IMU-driven synth. |
| `.docs/` | MCU reference PDFs and `imu_data_processing.md` (detailed math writeup of the whole IMU pipeline). |
| `.build/` | Build output (gitignored). |

## How the build wires together

`CMakeLists.txt` → `add_stm32_board(BOARDS/NUCLEO-F401RE NUCLEO-F401RE STM32F401xE)` from
[.cmake/stm32.cmake](.cmake/stm32.cmake), which:

- creates an INTERFACE library **`${BOARD_NAME}_config`** carrying include paths, `-mcpu=cortex-m4
  -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb`, and the defines `USE_FULL_LL_DRIVER` + `STM32F401xE`;
- globs `Core/Src/*.c` and `Drivers/STM32F4xx_HAL_Driver/Src/*.c` into the executable target `NUCLEO-F401RE`;
- globs `*FLASH.ld` in the board dir for the linker script.

`APPS/CMakeLists.txt` globs `src/*.c` into `apps` and links it into the board executable.

**Both source lists are globbed** — a new `.c` in `APPS/src/` needs no CMake edit, but you must
re-run `cmake --preset ...` (configure) for the glob to pick it up; a bare `--build` will not.

Any new library target must link `${BOARD_NAME}_config` to inherit the MCU flags and include paths.

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

I2C1 is still declared in the `.ioc` and initialised by `MX_I2C1_Init()` (100 kHz on PB6/PB7, AF4,
open-drain), but it has **no application alias** — the sensor-specific defines were removed along with
the driver. A new I2C device needs its own aliases added to `pin_definitions.h`.

The `.ioc` only declares USART2, I2C1 and SWD, so **`MX_GPIO_Init()` configures every other pin —
PA5 included — as analog**. An app that wants a GPIO must call `LL_GPIO_Init()` on it first; see
`blinky_init()`. The port clocks are already enabled for GPIOA/B/C/D/H, so only the pin mode needs
setting.

The deleted driver ran a **bit-banged I2C bus recovery** before enabling the peripheral, because an
I2C slave can be left holding SDA low across an MCU reset and then wedges the bus. Nothing does this
any more — a new I2C driver will need its own answer to that failure mode.

All peripheral access in `APPS/` is blocking polled LL (`while (!LL_..._IsActiveFlag_...())`) —
no interrupts or DMA are in use yet.

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
4. Braces on their own line, 4-space indent, `snake_case` for app functions, `MODULE_PascalCase` for
   static hardware helpers (e.g. `<Device>_ReadRegs`). Headers use `#ifndef`/`#define` guards.
   Note: the only examples of that helper convention lived in the deleted MPU6050 driver, so the
   remaining apps do not currently demonstrate it.

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
- Adding files to `APPS/src/` requires a reconfigure (glob), see above.
- `.cmake/` and `.docs/` are dot-prefixed on purpose — some tooling and shell globs skip them.
