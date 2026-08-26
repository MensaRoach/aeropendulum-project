# Implementation Plan — Multi-Board Restructure & Two-Person Workflow

**Created:** 2026-08-26 01:40 +02:00
**Status:** Phase 0 complete (commit `c6d44e6`). Phases 1–3 not started.
**Audience:** An LLM agent implementing this in a fresh session.

> Read [CLAUDE.md](../../../CLAUDE.md) before touching anything. Its conventions
> (LL drivers only, never the CubeMX CMake generator, no application code in generated files,
> spoiler discipline) all still apply and are not repeated here.

---

## 1. Why this change

Two people are now working on this project with **different boards**:

| Person | Board | State |
|---|---|---|
| Repo owner | NUCLEO-F401RE (STM32F401RET6, 84 MHz) | Fully working, tracked |
| Collaborator | STM32F407G-DISC1 (STM32F407VGT6, 168 MHz) | CubeMX export only, **untracked** |

They want to:
1. Share one portable MPU-6050 / ESC library between both boards.
2. Keep MCU-specific bus code separate from portable device code.
3. Work on separate branches and practise merging / conflict resolution.

### Critical technical finding (do not lose this)

**Both boards are STM32F4.** They share `stm32f4xx_ll_*.h`, the same **I2C v1** peripheral, and
identical register-level sequences. A `bus_i2c.c` written for one compiles and runs on the other
essentially unchanged.

What actually differs is small and already parameterised:

| Differs | Where it lives |
|---|---|
| Peripheral instance | `i2c_bus_t.instance` — runtime struct field |
| Pins | `pin_definitions.h` — per board |
| Clock (84 vs 168 MHz) | Read at runtime by `LL_I2C_ConfigSpeed` via PCLK1 |
| DMA stream/channel map | Per board — matters in Assignment 02, not 01 |
| Available timers | Matters for the ESC track only |

This is why the platform layer is keyed **per MCU family** (`DRIVERS/stm32f4/`), not per board.

---

## 2. Decisions already made (do not relitigate)

These were chosen explicitly by the user. Implement them as stated.

| Decision | Choice | Consequence |
|---|---|---|
| Platform layer organisation | **Per-family**, one shared `DRIVERS/stm32f4/` | Both people implement the *same* files independently |
| Assignment collaboration model | **Both write everything fully**, merge is a compare-and-synthesise session | Merges are not mechanical conflict resolution |
| Device library packaging | **`LIB/` in-repo, submodule-ready** | Each `LIB/<device>/` self-contained with its own `inc/`, `src/`, README |

---

## 3. ⚠ Sequencing constraint

**Phase 0 must land on `main` before either person creates a working branch.**

If they branch first and restructure second, both branches carry a different tree layout and every
subsequent merge fights about file paths on top of code. Land the migration as one commit on `main`,
then both branch from it.

Current state: branch `main`, clean except untracked `BOARDS/STM32F407G-DISC1/`.

---

## 4. Target structure

```
LIB/                        <- portable. Must not be able to see an STM32 header.
  drv_common/inc/           drv_status.h, i2c_transport.h, (later) pwm_channel.h
  mpu6050/   inc/ src/      mpu6050.h, mpu6050.c
  esc/       inc/ src/      (Assignment 05, does not exist yet)
DRIVERS/
  stm32f4/   inc/ src/      bus_i2c.*, bus_uart.*, (later) timer_pwm.*
APPS/                       <- wiring; picks instances and pins
BOARDS/
  NUCLEO-F401RE/
  STM32F407G-DISC1/
```

**Dependency direction:** `DRIVERS/stm32f4/` depends on `LIB/drv_common/`. This looks backwards but
is correct — the portable layer *defines* the interface (`i2c_transport.h`), the platform layer
*implements* it. Classic dependency inversion. Do not "fix" it by moving the headers into `DRIVERS/`.

---

## 5. Phase 0 — Migration (on `main`)

Pure move + rewire. **No behaviour change, no new implementation.** All driver bodies stay stubs
returning `DRV_ERR_STATE`.

### 5.1 File moves

Use `git mv` so history follows.

| From | To |
|---|---|
| `DRIVERS/inc/drv_status.h` | `LIB/drv_common/inc/drv_status.h` |
| `DRIVERS/inc/i2c_transport.h` | `LIB/drv_common/inc/i2c_transport.h` |
| `DRIVERS/inc/mpu6050.h` | `LIB/mpu6050/inc/mpu6050.h` |
| `DRIVERS/src/mpu6050.c` | `LIB/mpu6050/src/mpu6050.c` |
| `DRIVERS/inc/bus_i2c.h` | `DRIVERS/stm32f4/inc/bus_i2c.h` |
| `DRIVERS/src/bus_i2c.c` | `DRIVERS/stm32f4/src/bus_i2c.c` |
| `DRIVERS/inc/bus_uart.h` | `DRIVERS/stm32f4/inc/bus_uart.h` |
| `DRIVERS/src/bus_uart.c` | `DRIVERS/stm32f4/src/bus_uart.c` |

No `#include` lines need editing — all includes are by bare filename and resolve through
`target_include_directories`.

### 5.2 Split the board config target

In [.cmake/stm32.cmake](../../../.cmake/stm32.cmake), `add_stm32_board()` currently creates one
INTERFACE library `${BOARD_NAME}_config` carrying CPU flags **+** board include paths **+** defines.

Split it into two:

- **`${BOARD_NAME}_cpu`** — CPU compile options and link options only (from `cortex_flags()`).
  Nothing board-specific.
- **`${BOARD_NAME}_config`** — links `${BOARD_NAME}_cpu` PUBLIC, and adds the board include paths
  (`Core/Inc`, `Drivers/...`) and defines (`USE_FULL_LL_DRIVER`, `STM32F401xE` / `STM32F407xx`).

Everything that links `${BOARD_NAME}_config` today keeps working unchanged.

**This split is the enforcement mechanism, not a tidiness exercise.** `LIB/` links only `_cpu`, so a
device driver physically cannot `#include "stm32f4xx_ll_i2c.h"` — the path is absent and it becomes a
build error. Without it, the portability claim is only a comment.

### 5.3 CMake targets

**`LIB/CMakeLists.txt`**
```cmake
add_subdirectory(drv_common)
add_subdirectory(mpu6050)
```

**`LIB/drv_common/CMakeLists.txt`** — headers only:
```cmake
add_library(drv_common INTERFACE)
target_include_directories(drv_common INTERFACE inc)
```

**`LIB/mpu6050/CMakeLists.txt`**
```cmake
file(GLOB MPU6050_SOURCES CONFIGURE_DEPENDS "src/*.c")
add_library(mpu6050 STATIC ${MPU6050_SOURCES})
target_include_directories(mpu6050 PUBLIC inc)
# _cpu only, never _config: this is what keeps STM32 headers unreachable from here.
target_link_libraries(mpu6050 PUBLIC drv_common PRIVATE ${BOARD_NAME}_cpu)
```

**`DRIVERS/CMakeLists.txt`** — select the family subdirectory from the board manifest:
```cmake
string(TOLOWER "${MCU_FAMILY}" _family_dir)   # STM32F4 -> stm32f4
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_family_dir}")
    message(FATAL_ERROR "No platform drivers for MCU_FAMILY '${MCU_FAMILY}' (looked for DRIVERS/${_family_dir})")
endif()
add_subdirectory(${_family_dir})
```
Verify `MCU_FAMILY` is actually in scope at that point; if `add_stm32_board()` sets it in a different
scope, promote it to a cache variable or a normal variable in the parent scope.

**`DRIVERS/stm32f4/CMakeLists.txt`** — keep the target name `drivers` so `APPS` needs no change:
```cmake
file(GLOB DRIVERS_SOURCES CONFIGURE_DEPENDS "src/*.c")
add_library(drivers STATIC ${DRIVERS_SOURCES})
target_include_directories(drivers PUBLIC inc)
target_link_libraries(drivers PUBLIC ${BOARD_NAME}_config drv_common)
if(TARGET ${BOARD_NAME})
    target_link_libraries(${BOARD_NAME} PRIVATE drivers)
endif()
```
Note there is deliberately **no** include path to `BOARDS/${BOARD_NAME}` here — preserve that.

**Root `CMakeLists.txt`** — order matters, `apps` links both:
```cmake
add_stm32_board(${BOARD_NAME})
add_subdirectory(LIB)
add_subdirectory(DRIVERS)
add_subdirectory(APPS)
```

**`APPS/CMakeLists.txt`** — add the device library:
```cmake
target_link_libraries(apps PRIVATE drivers mpu6050)
```

### 5.4 Phase 0 verification

```bash
cmake --preset NUCLEO-F401RE-Debug-Local
cmake --build .build/NUCLEO-F401RE-Debug-Local --clean-first
```

Must build **warning-free**. Compare against the pre-migration baseline:

```
RAM:   1568 B      FLASH: 8964 B
```

Flash and confirm `APP_BLINKY` still blinks LD2 at 1 Hz. If the numbers moved, something other than a
move happened — find it before continuing.

Also prove the layering is enforced: temporarily add `#include "stm32f4xx_ll_i2c.h"` to
`LIB/mpu6050/src/mpu6050.c` and confirm the build **fails**. Remove it afterwards.

### 5.5 Execution record — 2026-08-26 02:09–02:12 +02:00

**Commit:** `c6d44e6` on `main`

All 8 `git mv` operations executed and tracked as `R100` renames. Six new CMake files
written. Four existing CMake files modified. `BOARDS/STM32F407G-DISC1/` (previously
untracked) also staged and committed in the same commit.

**Deviations from plan:**
- `git mv` requires destination directories to exist first on Windows; they were
  pre-created with `New-Item` before running the renames. No effect on git history.
- During the layering enforcement test, `mpu6050.c` was temporarily overwritten by the
  PowerShell restore step, breaking the rename tracking. Recovered by unstaging,
  re-checking out from HEAD, and re-running `git mv` after removing the stale destination.
  Final status `R100` confirmed before commit.
- `&&` is not a valid statement separator in PowerShell; all multi-step commands used `;`.

**Verification results:**

| Check | Result |
|---|---|
| Clean build, no warnings | ✅ |
| RAM 1568 B | ✅ (exact match) |
| FLASH 8964 B | ✅ (exact match) |
| `#include stm32f4xx_ll_i2c.h` in `LIB/mpu6050/src/mpu6050.c` → build error | ✅ `fatal error: stm32f4xx_ll_i2c.h: No such file or directory` |
| Restore → clean build | ✅ |

Stub discipline intact — no implementations added, all function bodies still return
`DRV_ERR_STATE`.

---

## 6. Phase 1 — Get the F407 building

`BOARDS/STM32F407G-DISC1/` currently holds a CubeMX LL export and nothing else.

**Confirmed from the `.ioc`:** LL mode, `Mcu.CPN=STM32F407VGT6`, I2C1 on **PB6/PB7** (same pins as the
F401), HCLK 168 MHz. Declares only `I2C1`, `NVIC`, `RCC`, `SYS` — **no USART, no DMA**. Startup file
`startup_stm32f407vgtx.s`, linker scripts `STM32F407VGTX_FLASH.ld` / `_RAM.ld`.

### 6.1 Create `BOARDS/STM32F407G-DISC1/board.cmake`
```cmake
stm32_board(
    MCU_FAMILY   STM32F4
    DEVICE       STM32F407xx
    CPU_PROFILE  cortex-m4f
    DRIVER_MODE  LL
    DEBUG_DEVICE STM32F407VG
)
```
Confirm `DEVICE` against what the CubeMX export's own defines use, and that `cortex-m4f` exists in
[.cmake/cortex_profiles.cmake](../../../.cmake/cortex_profiles.cmake).

### 6.2 Create `BOARDS/STM32F407G-DISC1/pin_definitions.h`

Mirror the F401 one. `IMU_I2C` = I2C1, `IMU_I2C_PORT` = GPIOB, SCL = PB6, SDA = PB7, AF4. Add
`STATUS_LED_PORT` / `STATUS_LED_PIN` — the DISC1 has four user LEDs on **PD12–PD15**; pick one and
confirm against the board's user manual. Add `TELEMETRY_USART` only once §6.4 is done.

### 6.3 Presets and launch config

Add to `CMakePresets.json`, mirroring the F401 entries: hidden `board-STM32F407G-DISC1`, then
`STM32F407G-DISC1-Debug` / `-Release` configure presets and matching build presets. Add a
`.vscode/launch.json` entry with `device: STM32F407VG`.

The user's machine-local `CMakeUserPresets.json` is gitignored — tell the user to add
`STM32F407G-DISC1-Debug-Local` there themselves; do not create it for them.

### 6.4 Tasks that belong to the collaborator, not to this agent

Flag these; do not attempt them.

1. **Add USART to the F407 `.ioc` in CubeMX.** Assignment 01 Part A is "print to your laptop" and the
   board currently has nothing to print through. Must be **LL**, and must not use the CubeMX CMake
   generator.
2. **Verify the serial path — this is the biggest practical risk.** On Nucleo, PA2/PA3 route to the
   ST-LINK virtual COM port. It is **not confirmed** that the STM32F407G-DISC1 wires its ST-LINK VCP
   to the target the same way. If it does not, a USB-TTL adapter is needed. Check this in week one,
   not mid-assignment.
3. **Do not commit `.mxproject`.** Repo convention keeps it per-machine (see CLAUDE.md gotchas). The
   export currently contains one. Check `.gitignore` covers it before staging the board folder.

### 6.5 Phase 1 verification

`APP_BLINKY` builds and runs on the F407. Both boards build from a clean tree.

---

## 7. Phase 2 — Documentation

Update to match the new tree. Keep every existing cross-reference working.

| File | Change |
|---|---|
| [CLAUDE.md](../../../CLAUDE.md) | Layout table gains `LIB/`; `DRIVERS/` row becomes per-family. Update "How the build wires together" and "Where code goes". Document the `_cpu` / `_config` split and *why* it exists. |
| [README.md](../../../README.md) | Directory structure + a short "two boards" note. |
| [DRIVERS/README.md](../../../DRIVERS/README.md) | Layering diagram gains the `LIB/` layer; state the family-keyed rule. |
| `LIB/README.md` | **New.** What belongs in `LIB/` vs `DRIVERS/`, and the submodule-ready rule. |
| [.docs/driver_development_plan.md](../../driver_development_plan.md) | Paths only; milestone IDs unchanged. |
| [.docs/driver_development_log.md](../../driver_development_log.md) | Add an entry recording the three decisions in §2 and the reasoning in §1. |

After editing, re-run the link check — every relative link in every `.md` must resolve.

---

## 8. Phase 3 — Homework

Current assignment: [.docs/homework/homework_01.html](../../homework/homework_01.html), a
self-contained styled page (logo embedded as base64, `<meta charset="UTF-8">` on line 1 — **keep it
there**, its absence previously caused mojibake).

Structure today: Stage S (4 tasks), Part A UART (3), Part B I2C (6), Part C MPU-6050 (7),
Part D verification (4) = **24 tasks**.

### 8.1 Path updates
- Parts A/B now write `DRIVERS/stm32f4/src/bus_uart.c` and `bus_i2c.c`
- Part C now writes `LIB/mpu6050/src/mpu6050.c`
- The "You will edit" / "You will not edit" boxes in Stage S

### 8.2 Add board-specific notes
Where a step differs between F401 and F407, say so inline — clock frequency, LED pin, the USART
situation, and the VCP caveat from §6.4.

### 8.3 Add Part E — Integrate
New final part, and the payoff of the whole architecture:

> Merge with your partner, synthesise one version, then **flash your partner's board with the
> merged driver.** If it runs, the layering is real. If it does not, you have found exactly where a
> board assumption leaked into the platform layer.

Update the task count (24 → ~27), the `<dd>N total</dd>` fact, and the `0 / N` progress label. These
three numbers must agree; there is a verification snippet in §10.

### 8.4 Add `.docs/homework/workflow.md`
**Open question — the user was asked whether this should be a separate page or a section inside
[.docs/homework/README.md](../../homework/README.md), and ran out of budget before answering. Ask
them, or default to a section in the README and mention the choice.**

Content:
- Branch naming (`feat/<name>/assignment-01` or similar)
- One commit per numbered task (already mandated by the assignment)
- **Merge cadence — the single most important part.** See §9.
- The synthesis-session protocol
- Useful commands: `git checkout --conflict=diff3`, `git diff <a>..<b> -- <path>`,
  `git merge --no-commit`

---

## 9. The collaboration model — and its failure mode

The user chose: **both people implement every file fully, independently; merges are
compare-and-synthesise sessions.** Implement the docs to support that, but state the risk plainly.

**Failure mode:** two independently-written 400-line files produce one giant conflict hunk. That is
not a merge, it is a rewrite with extra steps.

**Mitigation — cadence, not tooling. Merge per task, not per assignment.**

| Merge point | Conflict size | What it teaches |
|---|---|---|
| After A1 (deadline helper) | ~15 lines | The mechanics, on something readable whole |
| After A3 (UART write) | ~40 lines | Two valid approaches to one loop |
| After B4 (I2C read) | ~150 lines | The real thing — after four rehearsals |

Two supporting habits to document:
- `git checkout --conflict=diff3` shows the common ancestor. For from-scratch files it is empty —
  which is itself informative: it means nothing was shared.
- **After each synthesis, both people reset their branch onto the result.** Otherwise the branches
  diverge permanently and every later merge re-fights the previous one.

---

## 10. Verification checklist

Run before declaring any phase done.

```bash
# Both boards build clean and warning-free
cmake --preset NUCLEO-F401RE-Debug-Local && cmake --build .build/NUCLEO-F401RE-Debug-Local --clean-first
cmake --preset STM32F407G-DISC1-Debug-Local && cmake --build .build/STM32F407G-DISC1-Debug-Local --clean-first
```

Layering is enforced, not just documented:
- Adding an `#include "stm32f4xx_ll_i2c.h"` to `LIB/mpu6050/src/mpu6050.c` must **fail** the build.
- Adding an `#include "pin_definitions.h"` to `DRIVERS/stm32f4/src/bus_i2c.c` must **fail** the build.

Docs — every relative link resolves:
```bash
for f in README.md CLAUDE.md DRIVERS/README.md LIB/README.md .docs/*.md .docs/homework/*.md; do
  d=$(dirname "$f")
  grep -oE '\]\([^)#][^)]*\)' "$f" | sed 's/^](//; s/)$//' | while read -r l; do
    case "$l" in http*) continue;; esac
    t="${l%%#*}"; [ -z "$t" ] && continue
    [ -e "$d/$t" ] || echo "BROKEN in $f -> $l"
  done
done
```

Homework internal consistency (the three counts must agree):
```powershell
$h = [System.IO.File]::ReadAllText("<abs path>\.docs\homework\homework_01.html")
"cards : " + ([regex]::Matches($h,'<article class="task"')).Count
"boxes : " + ([regex]::Matches($h,'data-task="')).Count
"claim : " + ([regex]::Match($h,'<dd>(\d+) total</dd>')).Groups[1].Value
```

Driver stub discipline is intact — Phase 0–3 add **no** implementation:
```bash
grep -c "DRV_ERR_STATE" DRIVERS/stm32f4/src/*.c LIB/mpu6050/src/*.c
```

---

## 11. Traps specific to this repo

- **Windows + PowerShell text handling.** `Get-Content` without `-Encoding` decodes UTF-8 as cp1252
  and `Set-Content -Encoding utf8` writes the corruption back as valid UTF-8. This has already
  destroyed the homework page once. Use `[System.IO.File]::ReadAllText/WriteAllText` with an explicit
  `UTF8Encoding($false)`, or the Read/Write tools.
- **`git` is not on the Bash tool's PATH** in this environment; use the PowerShell tool for git.
- **`STM32F401RETX_FLASH.ld` must stay tracked** — `.cmake/stm32.cmake` globs `*FLASH.ld` and hard
  fails without it. Same now applies to `STM32F407VGTX_FLASH.ld`.
- **Never point a build at `*_RAM.ld`** — the image will not survive a power cycle.
- **Adding a `.c` file requires a CMake *reconfigure*,** not just a rebuild (sources are globbed with
  `CONFIGURE_DEPENDS`, which is unreliable across generators).
- **Spoiler discipline.** The driver rewrite is a deliberate learning exercise. Do not put MPU-6050
  register addresses, scale factors, the device address, timer clock frequencies, a timer/pin
  pairing, a target PWM frequency, or the ESC arming sequence into any learner-facing file. Working
  I2C code exists in git history (`11c4068^`) — **do not surface it to the learners.** See the
  Spoilers section of CLAUDE.md.
- **Keep `DRIVERS/` and `LIB/` free of `TODO` comments and teaching notes.** They are written as a
  published library; guidance lives in `.docs/homework/`.

---

## 12. Open questions — resolved 2026-08-26

1. **`workflow.md`** — does not exist yet; create as a new section appended to `.docs/homework/README.md`.
2. **Part E** — goes inside Assignment 01 (not a separate 01.5).
3. **Roadmap renumbering** — yes, update `.docs/homework/README.md` to reflect new paths and Part E.
