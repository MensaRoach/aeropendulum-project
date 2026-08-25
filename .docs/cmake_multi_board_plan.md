# CMake multi-board / multi-MCU refactor plan

**Status: implemented.** · **Date:** 2026-08-25 · **Baseline commit:** `078244e`

Shipped across `5d04274`, `ccf6b77` and `0e2fd23`. Kept as a historical implementation brief — the
reasoning in §4 (rejected alternatives) and §7 (why not build every board at once) is still the
justification for how the build works today, and §3.7's `CMakeUserPresets.json` template is still
what [README.md](../README.md) hands out.

> **Since diverged:** the root `CMakeLists.txt` snapshot in §3.4 and the `APPS/CMakeLists.txt` notes
> in §3.5 predate `DRIVERS/`. The build now adds `DRIVERS` before `APPS`, and `apps` links `drivers`.
> [CLAUDE.md](../CLAUDE.md) describes the current wiring; treat the snapshots below as the state at
> the time of the refactor, not as current.

This is an implementation brief. It was written to be executed by another agent. Read the whole
document before touching a file. Follow the phases in order — each one ends in a check you can
actually run.

---

## 1. Goal

Today the build can produce firmware for exactly one MCU: STM32F401RE. Adding a second board
(any other STM32 family, or a Cortex-M without an FPU) requires editing shared build logic and
would break the existing board.

After this refactor:

- **A board directory describes itself.** `BOARDS/<name>/board.cmake` declares the MCU, CPU
  profile, and family. Nothing MCU-specific stays in `.cmake/` or the root `CMakeLists.txt`.
- **Adding a board is: drop in the CubeIDE export + write ~6 lines + add two presets.** No edits
  to shared CMake logic.
- **Picking a board is one cache variable** (`BOARD_NAME`), settable by preset or `-D`.
- **A wrong board name fails loudly** with a list of the boards that do exist.

Non-goal: building two boards from one build directory. See §7 for why that is rejected.

---

## 2. What is actually hardcoded today

Read these before changing them: [.cmake/stm32.cmake](../.cmake/stm32.cmake),
[CMakeLists.txt](../CMakeLists.txt), [APPS/CMakeLists.txt](../APPS/CMakeLists.txt),
[.cmake/gcc-arm-none-eabi.cmake](../.cmake/gcc-arm-none-eabi.cmake).

| # | Location | Hardcoded thing | Why it blocks a second MCU |
|---|---|---|---|
| 1 | `stm32.cmake:4` | `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard` | Wrong for M0+/M0/M3 (no FPU) and for M7/M33 (different FPU). A mismatch produces the cryptic `uses VFP register arguments, output does not` link error. |
| 2 | `stm32.cmake:13-15` | `Drivers/STM32F4xx_HAL_Driver/Inc`, `.../CMSIS/Device/ST/STM32F4xx/Include` | Family-specific directory names. |
| 3 | `stm32.cmake:32` | `Drivers/STM32F4xx_HAL_Driver/Src/*.c` glob | Same. |
| 4 | `stm32.cmake:22` | `USE_FULL_LL_DRIVER` | Correct for this project, but it should come from the board, not the shared script. |
| 5 | `CMakeLists.txt:17` | `add_stm32_board(BOARDS/${BOARD_NAME} ${BOARD_NAME} STM32F401xE)` | `BOARD_NAME` is a cache variable but the **device define is a literal**. Changing `BOARD_NAME` alone yields a board built against the F401's device header. This is the headline bug. |
| 6 | `stm32.cmake:11-16, 32, 36, 45` | Include/glob paths are **relative** | They resolve against the *caller's* source dir. This works only because the function is called from the root `CMakeLists.txt`, and breaks silently if it is ever called from a subdirectory. |
| 7 | `stm32.cmake:51` | `-Wl,-Map=${CMAKE_PROJECT_NAME}.map` → `Aeropendulum.map` | Not per-board and not next to the `.elf`. Two boards would overwrite each other's map. |
| 8 | `APPS/CMakeLists.txt:8` | `../BOARDS/${BOARD_NAME}` | Relative, and depends on a global. Works, but should be absolute. |
| 9 | `CMakePresets.json` | Only `Debug` / `Release` | No board dimension at all. |
| 10 | `.vscode/launch.json:12` | `"device": "STM32F401RE"` | Debug/flash targets one MCU. |

Also worth knowing while working:

- **All three source lists are `file(GLOB)`** (board `Core/Src`, HAL `Src`, `APPS/src`). A new file
  needs a re-*configure*, not just `--build`. This is documented as a gotcha in
  [CLAUDE.md](../CLAUDE.md); Phase 1 removes it.
- `${BOARD_NAME}_config` resolves to the target `NUCLEO-F401RE_config` — **a dash in a target name
  is legal CMake**. Do not "sanitize" it; `APPS/CMakeLists.txt` links it by that exact name.
- The toolchain file keeps `CMAKE_C_FLAGS` ISA-neutral and lets `stm32.cmake` add `-mcpu` per
  target. **Preserve that split** — it is what makes per-board flags possible at all.

---

## 3. Target design

### 3.1 CPU profiles — `.cmake/cortex_profiles.cmake` (new)

One lookup, so a board names a CPU instead of retyping four flags it can typo.

```cmake
# cortex_flags(<profile> <out_var>)  -> sets <out_var> in the caller's scope
function(cortex_flags PROFILE OUT_VAR)
    if(PROFILE STREQUAL "cortex-m0")
        set(f -mcpu=cortex-m0     -mfloat-abi=soft)
    elseif(PROFILE STREQUAL "cortex-m0plus")
        set(f -mcpu=cortex-m0plus -mfloat-abi=soft)
    elseif(PROFILE STREQUAL "cortex-m3")
        set(f -mcpu=cortex-m3     -mfloat-abi=soft)
    elseif(PROFILE STREQUAL "cortex-m4")
        set(f -mcpu=cortex-m4     -mfloat-abi=soft)
    elseif(PROFILE STREQUAL "cortex-m4f")
        set(f -mcpu=cortex-m4  -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
    elseif(PROFILE STREQUAL "cortex-m7f")
        set(f -mcpu=cortex-m7  -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
    elseif(PROFILE STREQUAL "cortex-m7d")
        set(f -mcpu=cortex-m7  -mfpu=fpv5-d16    -mfloat-abi=hard)
    elseif(PROFILE STREQUAL "cortex-m33f")
        set(f -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
    else()
        message(FATAL_ERROR "Unknown CPU_PROFILE '${PROFILE}'. Known: cortex-m0, cortex-m0plus, "
                            "cortex-m3, cortex-m4, cortex-m4f, cortex-m7f, cortex-m7d, cortex-m33f")
    endif()
    set(${OUT_VAR} ${f} -mthumb PARENT_SCOPE)
endfunction()
```

`-mthumb` is appended to every profile — it is universal on Cortex-M.

> This is a sketch, not gospel. `if/elseif` or a `set(_profile_<name> ...)` variable-lookup table
> are both fine. The contract is the function signature and the error message.

### 3.2 Board manifest — `BOARDS/NUCLEO-F401RE/board.cmake` (new)

```cmake
stm32_board(
    MCU_FAMILY   STM32F4        # -> Drivers/STM32F4xx_HAL_Driver + CMSIS Device/ST/STM32F4xx
    DEVICE       STM32F401xE    # the CMSIS device define
    CPU_PROFILE  cortex-m4f
    DRIVER_MODE  LL             # LL -> USE_FULL_LL_DRIVER | HAL -> USE_HAL_DRIVER | BOTH -> both
    DEBUG_DEVICE STM32F401RE    # cortex-debug / probe name; see Phase 2
)
```

These values are confirmed against the current build: `Mcu.Family=STM32F4` and
`Mcu.UserName=STM32F401RETx` in the `.ioc`; `STM32F401xE` from `CMakeLists.txt:17`.

**Family directory rule:** `<FAMILY_DIR> = ${MCU_FAMILY}xx`. Verified against ST's layout for F4,
G0, G4, L4, H7, U5, WBA. Provide an optional `FAMILY_DIR` keyword to override it for anything
exotic; do not add per-family special cases up front.

Optional keywords, all with sane defaults. Implement them — they are cheap, and each one is a
reason the *next* board would otherwise have to edit shared code:

| Keyword | Default | Purpose |
|---|---|---|
| `FAMILY_DIR` | `${MCU_FAMILY}xx` | Override the derived driver/CMSIS directory token. |
| `LINKER_SCRIPT` | glob `*FLASH.ld` | Override when a board ships more than one FLASH script. |
| `SPECS` | `nano.specs` | e.g. `nosys.specs`, or full newlib. |
| `EXTRA_DEFINES` | *(empty)* | Board-level `-D`s. |
| `EXTRA_INCLUDE_DIRS` | *(empty)* | Extra include paths (BSP, middleware). |
| `PRINTF_FLOAT` | `OFF` | Adds `-u _printf_float` when `ON`. |

Parse with `cmake_parse_arguments`. **Hard-fail on a missing required keyword and on any unparsed
argument** — a typo'd keyword must not silently vanish.

### 3.3 `.cmake/stm32.cmake` (rewrite)

Public surface, three things:

```cmake
stm32_list_boards(OUT_VAR)     # dir names of every BOARDS/*/board.cmake
stm32_board(...)               # called *by* a board.cmake; records the manifest
add_stm32_board(BOARD)         # one argument now (was three)
```

`add_stm32_board(<board>)` must:

1. Resolve `BOARD_DIR = ${CMAKE_SOURCE_DIR}/BOARDS/<board>`. **Every path from here on is
   absolute** — this fixes issue #6.
2. Error if `${BOARD_DIR}/board.cmake` is missing, and **list the boards that do exist**:
   `Unknown board 'Bogus'. Available: NUCLEO-F401RE`.
3. `include()` the manifest, which calls `stm32_board(...)` and populates the manifest values.
4. `cortex_flags(${CPU_PROFILE} TARGET_FLAGS)`.
5. Create the `INTERFACE` library `${BOARD}_config` — same shape as today, values from the manifest:
   - includes: `Core/Inc`, `Drivers/${FAMILY_DIR}_HAL_Driver/Inc`, `.../Inc/Legacy`,
     `Drivers/CMSIS/Device/ST/${FAMILY_DIR}/Include`, `Drivers/CMSIS/Include`, plus
     `EXTRA_INCLUDE_DIRS`
   - defines: the driver-mode define(s), `${DEVICE}`, plus `EXTRA_DEFINES`
   - `target_compile_options` **and** `target_link_options` = `${TARGET_FLAGS}`
6. Glob `Core/Src/*.c` + `Drivers/${FAMILY_DIR}_HAL_Driver/Src/*.c` and `Core/Startup/*.s`, both
   with `CONFIGURE_DEPENDS`.
7. `add_executable(<board> ...)`, link `${BOARD}_config` and `m`.
8. Linker script: manifest override, else glob `${BOARD_DIR}/*FLASH.ld`. Keep the `FATAL_ERROR` —
   **name the board in the message**, and keep its intent (see §8).
9. Link options: `-T<script>`, `--specs=${SPECS}`, `-Wl,--gc-sections`,
   `-Wl,--print-memory-usage`, and the map file as
   `-Wl,-Map=$<TARGET_FILE_DIR:${BOARD}>/${BOARD}.map` — per-board, and next to the `.elf` instead
   of `Aeropendulum.map` in the build root. **This renames an output file that
   [CLAUDE.md](../CLAUDE.md) documents; Phase 3 covers the doc update.**

Keep the function re-entrant (no globals, everything per-target) even though only one board is
configured at a time. It costs nothing and keeps §7's option open.

### 3.4 Root `CMakeLists.txt`

```cmake
include(.cmake/cortex_profiles.cmake)
include(.cmake/stm32.cmake)

stm32_list_boards(AVAILABLE_BOARDS)
set(BOARD_NAME "NUCLEO-F401RE" CACHE STRING "Selected board")
set_property(CACHE BOARD_NAME PROPERTY STRINGS ${AVAILABLE_BOARDS})   # dropdown in CMake Tools / cmake-gui

add_stm32_board(${BOARD_NAME})
add_subdirectory(APPS)
```

**Keep the variable named `BOARD_NAME`.** Do not rename it to `BOARD` — `APPS/CMakeLists.txt`, the
existing `CMakeUserPresets.json` on this machine, and the docs all use that spelling, and the
rename buys nothing.

### 3.5 `APPS/CMakeLists.txt`

Minimal change; the app layer is already board-agnostic and `pin_definitions.h` already lives
per-board. Only:

- `file(GLOB APPS_SOURCES CONFIGURE_DEPENDS "src/*.c")`
- `target_include_directories(apps PRIVATE ${CMAKE_SOURCE_DIR}/BOARDS/${BOARD_NAME})` — absolute
- leave the `${BOARD_NAME}_config` link and the `if(TARGET ...)` guard exactly as they are

### 3.6 Presets

Cross-product without duplication: one hidden preset per board, carrying only the cache variable.

```jsonc
// hidden
{ "name": "board-NUCLEO-F401RE", "hidden": true,
  "cacheVariables": { "BOARD_NAME": "NUCLEO-F401RE" } }

// concrete
{ "name": "NUCLEO-F401RE-Debug",   "displayName": "NUCLEO-F401RE (Debug)",
  "inherits": ["Debug",   "board-NUCLEO-F401RE"] }
{ "name": "NUCLEO-F401RE-Release", "displayName": "NUCLEO-F401RE (Release)",
  "inherits": ["Release", "board-NUCLEO-F401RE"] }
```

Add matching `buildPresets`. `binaryDir` is already `${sourceDir}/.build/${presetName}` on the
`default` base, so each board+config gets its own build tree automatically.

**Leave `Debug` and `Release` concrete and board-less.** They fall back to the `BOARD_NAME` cache
default, which keeps `cmake --preset Debug` (documented in README and CLAUDE.md) and the existing
untracked `Debug-Local` / `Release-Local` presets working across this refactor. Do not make them
hidden.

Also add `"CMAKE_EXPORT_COMPILE_COMMANDS": "ON"` to the `default` base preset — Phase 0's
verification depends on it, and it helps clangd.

Adding a board later = one hidden preset + two concrete + two build presets. Mechanical.

### 3.7 `CMakeUserPresets.json` template (README only — the file itself is gitignored)

Restructure so the local toolchain PATH is written once:

```jsonc
{ "name": "local-toolchain", "hidden": true,
  "environment": { "PATH": "C:/ST/STM32CubeCLT_1.21.0/GNU-tools-for-STM32/bin;.../Ninja/bin;.../CMake/bin;$penv{PATH}" } }

{ "name": "NUCLEO-F401RE-Debug-Local",
  "inherits": ["NUCLEO-F401RE-Debug", "local-toolchain"] }
```

Multiple inheritance is valid in presets v3. Keep the existing README warning about the `;` vs `:`
separator — it is the highest-value line in that section.

### 3.8 `.vscode/`

- `launch.json`: one configuration per board, differing only in `name` and `device`. Keep
  `"executable": "${command:cmake.launchTargetPath}"` — it already follows the configured build dir.
- `settings.json`: `"cmake.buildDirectory": "${workspaceFolder}/.build/${buildType}"` is **stale**.
  It is ignored when driving from presets (the preset's `binaryDir` wins) and its `${buildType}`
  has no board dimension. Remove it, or leave it with a one-line comment saying presets override
  it. Do not invent a `${boardName}` expansion — CMake Tools has no such variable.

---

## 4. Rejected alternatives

| Option | Why not |
|---|---|
| Central MCU→flags table in `stm32.cmake` | Every new board edits a shared file. The manifest keeps the knowledge next to the board it describes. |
| Derive everything from the `.ioc` (`Mcu.Family`, `Mcu.UserName`) | Family → directory is derivable; the **device define is not**. `STM32F401RETx` → `STM32F401xE` needs a flash/package decode table with real exceptions (`STM32H743xx`, `STM32G0B1xx`). A parser that is right 80% of the time is worse than six explicit lines. |
| Build all boards in one build tree | See §7. |
| Drop the `apps` static lib, compile sources straight into the executable | Would work, but churns the structure [CLAUDE.md](../CLAUDE.md) documents, for no gain while one board is active. |

---

## 5. Phases

Do them in order. Do not start a phase before the previous one's check passes.

### Phase 0 — Baseline (do this first; it is the whole verification story)

- [ ] Configure and build current `main` unchanged:
      `cmake --preset Debug-Local && cmake --build .build/Debug-Local`
- [ ] Copy `.build/Debug-Local/compile_commands.json` to the scratchpad as `baseline-cc.json`.
      If the file is absent, configure once with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` first.
- [ ] Record the `arm-none-eabi-size .build/Debug-Local/NUCLEO-F401RE.elf` output.
- [ ] Save `.build/Debug-Local/Aeropendulum.map` alongside it.

Everything after this is judged against those three artifacts.

### Phase 1 — Build system

- [ ] Add `.cmake/cortex_profiles.cmake` (§3.1)
- [ ] Rewrite `.cmake/stm32.cmake` (§3.3)
- [ ] Add `BOARDS/NUCLEO-F401RE/board.cmake` (§3.2)
- [ ] Update the root `CMakeLists.txt` (§3.4)
- [ ] Update `APPS/CMakeLists.txt` (§3.5)

**Check — the refactor must be flag-for-flag invisible:**

- [ ] Delete `.build/Debug-Local`, reconfigure from scratch, build. It succeeds.
- [ ] Diff the new `compile_commands.json` against `baseline-cc.json`. Every `-mcpu`, `-mfpu`,
      `-mfloat-abi`, `-D` and `-I` must match. Only path spellings (relative → absolute) may
      differ. **Any changed compile flag is a bug in your manifest, not an improvement.**
- [ ] `arm-none-eabi-size` output matches the baseline.
- [ ] A `NUCLEO-F401RE.map` now sits next to the `.elf`.
- [ ] `cmake --preset Debug-Local -DBOARD_NAME=Bogus` fails with a message naming `NUCLEO-F401RE`.
- [ ] Temporarily flip `CPU_PROFILE` to `cortex-m0plus` in `board.cmake`, reconfigure, confirm the
      flags in `compile_commands.json` change accordingly — **then revert**. This is how you prove
      the plumbing works without a second board.

### Phase 2 — Presets and editor config

- [ ] `CMakePresets.json` per §3.6 (including `CMAKE_EXPORT_COMPILE_COMMANDS`)
- [ ] `.vscode/launch.json` per §3.8
- [ ] `.vscode/settings.json` per §3.8

**Check:**

- [ ] `cmake --preset NUCLEO-F401RE-Debug-Local` configures into
      `.build/NUCLEO-F401RE-Debug-Local/` (after adding that preset to the local
      `CMakeUserPresets.json`)
- [ ] `cmake --preset Debug-Local` still works unchanged
- [ ] `cmake --list-presets` shows both families

### Phase 3 — Documentation (not optional)

[CLAUDE.md](../CLAUDE.md) describes the current build in enough detail that this refactor makes
several sections factually wrong. Update:

- [ ] **Build & flash** — new preset names; the `Aeropendulum.map` → `NUCLEO-F401RE.map` rename
- [ ] **Layout** table — add `BOARDS/<board>/board.cmake` and `.cmake/cortex_profiles.cmake`
- [ ] **How the build wires together** — the `add_stm32_board(BOARDS/... NAME STM32F401xE)` line is
      now `add_stm32_board(NUCLEO-F401RE)`; flags and defines now come from the manifest
- [ ] **Gotchas** — the "adding files to `APPS/src/` requires a reconfigure" gotcha is **removed**
      by `CONFIGURE_DEPENDS`; delete or rewrite it. Keep the `*FLASH.ld` gotcha (see §8).
- [ ] **New section: "Adding a board"** — the recipe in §6
- [ ] [README.md](../README.md) — Directory Structure, the `CMakeUserPresets.json` template (§3.7),
      the Building section, and the same "Adding a board" recipe

---

## 6. The recipe this refactor is supposed to produce

Write this into both docs. If executing it would require touching `.cmake/` or the root
`CMakeLists.txt`, the refactor is not finished.

1. Generate the board in STM32CubeIDE with **LL drivers**. Copy the export to `BOARDS/<board>/`.
   (Never the CubeMX CMake generator — see the conventions in [CLAUDE.md](../CLAUDE.md).)
2. Write `BOARDS/<board>/board.cmake` (§3.2) and `BOARDS/<board>/pin_definitions.h`.
3. Add the hidden preset, two concrete presets, and two build presets (§3.6).
4. Add a `.vscode/launch.json` configuration with that board's `device`.
5. `cmake --preset <board>-Debug && cmake --build .build/<board>-Debug`

---

## 7. Why not build every board at once

CMake *can* do it — all ISA flags are already per-target via `${BOARD}_config`, and the rewrite
keeps `add_stm32_board` re-entrant, so nothing here forecloses it. It is rejected for now because:

- VS Code CMake Tools' single-configure model, the `.build/<presetName>` layout, and
  `${command:cmake.launchTargetPath}` all assume one active configuration.
- Mixing `-mcpu` values in one build tree is a known footgun for multilib selection and
  `try_compile` behaviour, and debugging that is not what this project is for.
- With one board on the bench, the payoff is zero.

If it is ever wanted: make `apps` a per-board target (`apps_${BOARD_NAME}`) and loop
`add_stm32_board` over `AVAILABLE_BOARDS`. Nothing else in this design blocks it.

---

## 8. Do not do these

- **Do not run CubeMX**, and do not touch `BOARDS/NUCLEO-F401RE/Core/` or `Drivers/`.
- **Do not create a placeholder or fake second board directory** to "test" the refactor. Verify
  with the temporary `CPU_PROFILE` flip in Phase 1 instead.
- **Do not introduce HAL** or change `DRIVER_MODE` away from `LL`.
- **Do not rename `BOARD_NAME`.**
- **Do not delete `STM32F401RETX_FLASH.ld`, and do not add a `*FLASH.ld` rule to `.gitignore`** —
  `stm32.cmake` globs for it and hard-fails without it, so a clone missing it cannot configure at
  all. Also do not "fix" a missing script by pointing at `STM32F401RETX_RAM.ld`; that links the
  image to run from RAM and it will not survive a power cycle.
- **Do not move `-mcpu`/`-mfpu` into `CMAKE_C_FLAGS`** in `.cmake/gcc-arm-none-eabi.cmake`. Those
  flags must stay per-target; the toolchain file stays ISA-neutral.
- **Do not "fix" `CMAKE_ASM_FLAGS`** in the toolchain file. It is composed from `CMAKE_C_FLAGS`
  before the append on the following line, which is almost certainly unintended — but it is
  pre-existing, unrelated to multi-board, and changing it would perturb the Phase 1 flag diff.
  Note it for later; leave it alone here.
- **Do not add `.hex`/`.bin` post-build steps, a generated `board_info.json`, or `svdFile`
  support.** All defensible, none in scope. Separate change.
