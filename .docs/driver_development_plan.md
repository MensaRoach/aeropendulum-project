# Driver Development Plan — ESC & IMU From Scratch

**Purpose:** structure the from-scratch rewrite of the ESC and IMU drivers as a learning
exercise. This is a process document, not a spec — it says what to figure out and in what
order, not what the answers are. See the note at the bottom on spoiler sources.

**Status:** planning. Both drivers were deleted / never existed as of the MPU6050 removal
commit; see [CLAUDE.md](../CLAUDE.md) for current firmware state.

---

## Why two tracks, not one

The IMU and the ESC are different kinds of problem, and treating them the same is a trap:

- **IMU = data acquisition** over a bus (I2C1) that already works elsewhere in this repo.
  The difficulty is in the register map, configuration semantics, and sample timing.
- **ESC = timing generation** on a peripheral (a timer) that does not exist anywhere in this
  project yet. The difficulty is in the timer hardware itself and in the safety state machine
  around it.

A good approach on one does not automatically transfer to the other.

---

## Phase 0 — Driver contract (do this first, for both drivers)

Pure design, no datasheet lookup required. Answer once, apply to both drivers. This is the
single highest-value part of the whole plan and the first thing worth reviewing.

- [ ] **Ownership.** Does the driver configure its own bus/timer, or does it assume the
      peripheral is already initialised? Stress-test the answer: what happens when a second
      device needs to share that same bus?
- [ ] **Blocking or not.** A 250 Hz control loop has a 4 ms budget; a blocking multi-byte I2C
      read can eat a third of it. Blocking is a fine place to *start*, but decide the API shape
      (`read()` vs. `start()` / `is_ready()` / `get()`) so that moving to non-blocking later is
      not a rewrite.
- [ ] **Raw counts or physical units.** Scaling inside the driver hides the sensor/actuator
      configuration from the caller; scaling above the driver duplicates knowledge of that
      configuration. Consider how the answer changes if range/resolution becomes runtime-
      configurable.
- [ ] **Error surfacing.** Enumerate everything that can actually go wrong. For each: can the
      caller do anything useful with that specific error? If not, ask why it's being returned
      at all rather than handled internally.
- [ ] **`init()` contract.** If `init()` fails, what state is the driver left in? Is calling
      `read()`/`write()` after a failed `init()` defined behaviour, or undefined?

Write the answers down before implementing either driver.

---

## IMU track

- [ ] **I0 — Get the register map.** `.docs/IMU/MPU6050-DataSheet.pdf` is the *product
      specification* (electrical characteristics, packaging, performance). Register
      descriptions live in a **separate** InvenSense register-map document. Most of the real
      work is in the document you don't currently have — get it first.
- [ ] **I1 — Refactor the bus plumbing, don't rewrite it.** The deleted driver's register
      read/write helpers and I2C bus-recovery logic are recoverable from git history
      (`git log -- APPS/src/mpu6050_telemetry.c`) and are reusable, sensor-agnostic I2C
      groundwork. First milestone: a clean driver skeleton with I2C read/write primitives and
      **no sensor-specific behaviour yet**. Exercises the Phase 0 contract on known-solid
      ground before register-map unknowns enter the picture.
- [ ] **I2 — Configuration coverage.** From the register map's configuration chapter,
      enumerate everything that affects the numbers coming out of the device. For each: does
      it belong in the driver's runtime config, or is it fixed at init? Then check — which
      reset-default values would the driver be inheriting by accident if left unset?
- [ ] **I3 — Sampling model.** Does the device expose a data-ready signal, is it wired to a
      free STM32 pin, and is it worth using? What does polling cost vs. an interrupt? If
      sampling faster than the device actually updates, how would that be detected rather than
      just assumed away?
- [ ] **I4 — Calibration.** Gyro bias at minimum. When does it run (boot only? on demand?).
      How is "the device is actually still" established? Where does a calibration result live
      across a reset, and how is a stored value known to still be valid?

### IMU verification ladder
Each rung should pass before moving to the next.

- [ ] Identity/WHOAMI register returns the expected device ID
- [ ] At rest, accelerometer magnitude ≈ 1 g in **any** static orientation (not just one —
      this catches scaling and axis-assignment bugs a single-orientation test misses)
- [ ] Six-face test: each axis reads ±1 g pointed up and pointed down
- [ ] Gyro ≈ 0 at rest; integrating a deliberate known rotation lands near the expected angle
- [ ] Measured sample interval (scope/logic analyzer on a toggled GPIO, or timestamps in the
      telemetry frame) matches the intended rate — don't trust the nominal number
- [ ] Sign convention confirmed empirically: a positive reading on an axis means what the
      downstream math assumes it means

---

## ESC track

**Safety gate before any of this touches a live motor:** a clamped stand, a physical prop
guard, and a kill switch in the battery line. Everything through E3 below should be exercised
on a **bare motor with no propeller** — verifies essentially all of the driver behavior at
near-zero risk.

- [ ] **E0 — Learn the timer peripheral.** From the general-purpose timer chapter of RM0368
      (`.docs/MCU/rm0368-...pdf`), derive rather than look up:
  - [ ] the clock path from system clock to the timer counter (there is a non-obvious step on
        the APB branch — find it, don't take a number on faith)
  - [ ] how prescaler + auto-reload + compare together define period and pulse width
  - [ ] what PWM output-compare mode actually does at the hardware level
  - [ ] why preload/shadow registers exist, and what breaks if the compare value is written at
        the wrong moment
  - [ ] which timer/pin pairing is available, cross-referencing the STM32F401RE alternate-
        function table against what [pin_definitions.h](../BOARDS/NUCLEO-F401RE/pin_definitions.h)
        already claims. Pin config goes through CubeMX — but never the CubeMX CMake generator
        (see CLAUDE.md conventions).
- [ ] **E1 — Prove the timing before the ESC is in the loop at all.** Generate the waveform
      and *measure* it, not just trust the register values. In descending order of preference:
      logic analyzer/scope; a second timer in input-capture mode measuring the first (no extra
      hardware, and a genuinely good exercise on its own); or, as a last resort, an absurdly
      slow period driven onto the status LED to eyeball duty cycle by eye. Done when measured
      pulse width tracks commanded pulse width across the full range, with known resolution
      and jitter.
- [ ] **E2 — Protocol layer.** Decide which signalling protocol to implement and write down
      *why* — there are real tradeoffs between the options (see the LANRC ESC's supported
      protocol list). Then work out what the ESC expects at power-up from the BLHeli_S
      documentation, not by trial and error — guessing here just produces a motor that beeps
      at you with no way to tell which assumption was wrong.
- [ ] **E3 — State machine and safety.** The part most often skipped, and the part that
      matters most. Sketch states and transitions on paper before writing code. Work through
      explicitly:
  - [ ] what state does the driver boot into
  - [ ] what does arming actually require
  - [ ] what happens on an MCU reset while the ESC stays powered
  - [ ] what happens if the loop feeding the driver stalls
  - [ ] how fast is throttle allowed to change
  - [ ] what conditions force a disarm

  Every one of these is a failure mode that will eventually happen for real, not a
  hypothetical.
- [ ] **E4 — Bare motor.** Only after E0–E3: prop mounted, on a clamped and guarded stand.

### ESC verification ladder
- [ ] Commanded pulse width == measured pulse width, across the full range
- [ ] Arms reliably from cold power-up, repeatedly, not just once
- [ ] Behaves correctly after an MCU reset with the ESC still powered — test this deliberately,
      don't assume
- [ ] Throttle response is monotonic and repeatable on a bare, guarded motor
- [ ] Every failsafe path fires when deliberately provoked. A failsafe that has never been
      triggered is not a verified failsafe.

---

## Cross-cutting practices

- [ ] **One test app per driver**, using the existing `app_selector.h` pattern — an app that
      exercises nothing but the one driver. Keeps the driver API honest: if the test app has
      to reach around the API to do something, the API is wrong. Remember: a new `.c` in
      `APPS/src/` needs a CMake **reconfigure**, not just a rebuild (glob-based sources).
- [ ] **Host command path.** Both drivers eventually benefit from one, and it makes ESC
      testing much less painful than hand-coded test sequences. `cobs_decode()` already exists
      in [cobs.c](../APPS/src/cobs.c), unused. Decide whether to build the command path now or
      defer it.
- [ ] **Decisions log.** One line per non-obvious choice plus the reasoning behind it, per
      driver. This is what turns a later review into feedback on the *thinking*, not just the
      syntax.
- [ ] **One branch per driver, one commit per milestone above.** Makes the eventual review
      read as a sequence of reasoned steps instead of one large diff.

---

## Suggested order

**IMU first**, ESC second — not a hard dependency, just the better default:

1. It's a refactor of previously-working code, so there's no hardware risk while the Phase 0
   contract is being shaken out.
2. The ESC track needs mechanical hardware (arm, mount, guard, stand) that doesn't exist yet;
   the IMU track needs none of that.
3. Establishing the driver contract on the easier problem first makes it easier to apply
   correctly to the harder one.

If splitting the work between people, the two tracks are genuinely independent and can run in
parallel.

---

## Spoiler warning

[`feasibility_analysis.md`](feasibility_analysis.md) — written before this plan, as a
feasibility pass — gives away several answers this plan asks to be derived independently: a
specific MPU6050 configuration register, the timer clock frequency, a candidate timer/pin
pairing, a target PWM frequency, and roughly what the ESC arming sequence needs.

**Skip sections 5 ("The real risk: vibration"), 6 ("Control feasibility"), and 7 ("Firmware
gap analysis") of that document** until Phase 0 through E2/I3 above have been worked through
independently. Sections 1–4, 8, 9, and 10 (parts inventory, sizing, BOM, safety, build order)
don't spoil anything driver-specific and are safe to read any time.

[`imu_data_processing.md`](imu_data_processing.md) is also a spoiler source for the IMU track
specifically — it documents the device address, several register addresses (including the
DLPF register), the burst-read start register, and the scaling constants. Written pre-rewrite
as a data-pipeline reference; treat it the same way.
