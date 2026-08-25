# Aeropendulum Feasibility Analysis

**Date:** 2026-08-25 · **Firmware baseline:** `acea59e` · **Hardware:** NUCLEO-F401RE + SpeedyBee 1404 V3 4600 KV + HQProp T3.5×2×3 + LANRC 45 A BLHeli_S

> **Summary:** The project is feasible with the parts already in `.docs/`. The motor is roughly **12× stronger** than the pendulum needs. The two decisions that determine whether it works are **battery voltage** (use 2S, not 4S) and **how the angle is measured through propeller vibration** (add a pivot encoder). Neither requires replacing existing components.

A rendered version of this document is published at
<https://claude.ai/code/artifact/68d6c6ac-59f0-4f03-9d7d-362985d1c222>.

---

## 1. Verdict by subsystem

| Subsystem | Verdict | Note |
|---|---|---|
| Overall project | **Go, with changes** | No dead ends. One powertrain decision and one sensing decision. |
| Motor · prop · ESC | **Overpowered** | Quadcopter-class hardware. De-rate by voltage, not by throttle limits. |
| Battery voltage | **Decide first** | 4S puts the whole operating band under 20 % throttle. 2S is the match. |
| STM32F401RE | **Ample** | 84 MHz + FPU against a ~1 Hz plant. Timers, DMA, pins all free. |
| Angle sensing (MPU6050 alone) | **Top risk** | Prop vibration aliases into the control band; DLPF is at its reset value. |
| Control design | **Well-conditioned** | Lightly damped 2nd-order plant, 100× sample-rate margin. |
| Firmware | **Mostly unbuilt** | No timer, no PWM, no fixed timebase, no command path, no failsafe. |
| Mechanical & power BOM | **Incomplete** | Arm, bearings, base, battery, bulk cap, prop guard all missing. |
| Safety | **Guard required** | ~170 km/h tip speed even at 2S. |

All mass-dependent numbers below are estimates until the parts are weighed.

---

## 2. Component inventory

### SpeedyBee 1404 V3, 4600 KV
| | |
|---|---|
| Stator | 14 × 4 mm |
| Bell diameter | 18.1 mm |
| Shaft | Ø1.5 mm, press-fit |
| Mounting | 4 × M2 on Ø9 mm |
| Peak (4S, tested) | 431.5 gf @ 12.85 A, 201.9 W, 47 879 RPM |
| Internal temp at peak | 80.8 °C (30 °C ambient) |

### HQProp T3.5×2×3
3.5″ diameter, 2.0″ pitch, 3 blades, 1.5 g, 1.5 / 1.9 mm bore.

### LANRC 45 A
BLHeli_S on EFM8BB21F16G, 2S–6S, 45 A continuous / 50 A for 10 s.
Supports PWM, Oneshot125, Multishot, DShot150/300/600. **No BEC mentioned — assume none.**

### Compatibility: passes
1.5 mm shaft into a 1.5 mm bore; M2 mounting; BLHeli_S input runs at 3.3 V logic, so the
STM32 drives the signal line directly with no level shifter. Nothing is mismatched — the
problem is scale, not fit.

### ⚠ The thrust table is for a different propeller
The manufacturer's figures were measured on a **GemFan 3016R** (3.0″ × 1.6″). Yours is
3.5″ × 2.0″, 3-blade. At the *same throttle setting* the larger disc is a heavier load:
roughly **30 % lower RPM, 25–40 % more thrust, 60–90 % more current**. Every figure below
derived from that table is therefore optimistic on current and conservative on thrust.
Treat it as a curve shape, not a calibration.

---

## 3. Sizing: one equation decides everything

Uniform arm of mass `m_a`, length `L`, pivoted at one end, motor assembly `m_t` at the tip.
Thrust acts perpendicular to the arm. Moments about the pivot at angle θ from straight down:

```
T · L = (m_a/2 + m_t) · g · L · sin θ

  ⟹   T = M_eff · g · sin θ ,   M_eff = m_a/2 + m_t
```

**Arm length cancels.** Thrust demand depends only on effective mass and angle; `L` sets the
dynamics, not the thrust budget. The worst case in the whole envelope is the arm held
horizontal (θ = 90°).

### Representative build (300 mm arm, ESC mounted near the pivot)

| Item | Position | Mass | Adds to M_eff |
|---|---|---:|---:|
| Motor 1404 | tip | 15 g | 15 g |
| Prop 3.5×2×3 | tip | 1.5 g | 1.5 g |
| Motor mount + fasteners | tip | 8 g | 8 g |
| Carbon tube, 300 mm | distributed | 20 g | 10 g |
| ESC + wiring | at pivot | 12 g | ≈ 0 g |
| **M_eff** | | | **≈ 35 g** |

So the arm needs **35 gf to sit horizontal**. Against 431 gf available on 4S, that is a
**12× margin** — which is the problem, not the reassurance.

---

## 4. Headline finding: pick 2S, not 4S

Thrust ∝ RPM², and at fixed throttle % RPM ∝ battery voltage, so the 4S curve rescales by
`(V/16.0)²`. Fitting the tabulated data gives `T ≈ T_max · u^1.58` for throttle fraction `u`.

| Pack | Max thrust | Max current | Throttle at 35 gf hover | Consequence |
|---|---:|---:|---:|---|
| 4S · 16.0 V | 431 gf | 12.9 A | **≈ 20 %** | Entire operating band below the characterised region, in the ESC's coarse low end. Motor already hits 80.8 °C at full throttle *with a smaller prop*. |
| 3S · 11.4 V | 219 gf | 6.5 A | ≈ 31 % | Workable. Better with ~40 g tip ballast, which moves hover to ~50 %. |
| **2S · 7.6 V** | **98 gf** | **2.9 A** | **≈ 52 %** | **The match.** Hover mid-range where thrust is smooth and repeatable, current trivial, motor cool, and full throttle still 2.8× hover. |

### ⚠ Do not run this combination at 4S or above
A 4600 KV 1404 is designed for 2.5–3″ props on 4S, or 3–3.5″ on 2–3S. A 3.5″ tri-blade on
4S is a textbook over-prop condition: RPM drops, torque and current climb, and the copper
heats faster than the bell can shed it. The ESC will shrug it off (45 A against ~20 A); the
motor will not.

### Why the operating point matters more than the headroom
Control gain is the slope of the thrust curve, `dT/du = 1.58 · T/u`. At 20 % throttle you
ride the steep, uncharacterised bottom of the curve where BLHeli_S commutation is least
stable. At 52 % the curve is smooth and a given PWM step yields a repeatable thrust step —
which is what makes a PID tuneable.

---

## 5. Top risk: vibration will forge the angle

The telemetry chain works on a bench. It has never seen a propeller.

At the 2S hover point the prop turns ~10 000 RPM → imbalance tone at **167 Hz**, blade
passing at **500 Hz**. Meanwhile in the current firmware:

- [`mpu6050_telemetry.c:159`](../APPS/src/mpu6050_telemetry.c) writes `ACCEL_CONFIG` (0x1C) and
  `GYRO_CONFIG` (0x1B) but **never writes `CONFIG` (0x1A)**, so `DLPF_CFG` stays at its reset
  value of 0 — the widest setting, 260 Hz accelerometer bandwidth.
- The loop samples at ~100 Hz (`LL_mDelay(10)`), so everything above 50 Hz folds back in.

167 Hz sampled at 100 Hz aliases to **33 Hz**. Worse, the alias is not stationary: as
throttle changes, RPM changes, and the alias *sweeps* down through the control bandwidth.
When it crosses 0–2 Hz it is indistinguishable from real pendulum motion.

**Scale:** 0.2 g of residual vibration reads as ~**11° of apparent tilt**. A 1404 with an
unbalanced 3.5″ prop routinely produces several g at the motor.

### Mitigations, ordered by effect

| Fix | Cost | Effect |
|---|---|---|
| **Pivot angle encoder** — AS5600 (I²C, 12-bit, addr 0x36, no conflict with MPU6050 at 0x68) on the hinge shaft | a few € + magnet | **Eliminates the problem.** Absolute, vibration-immune, no filter lag in the loop. Also provides ground truth to validate the IMU against. |
| Set `DLPF_CFG` = 4–5 (21 / 10 Hz) via `MPU6050_WriteReg(0x1A, …)` | one line | Large. Cannot stop >500 Hz aliasing in the analog front end, but at 2S RPM most energy is below that. |
| Soft-mount the IMU (foam / gel tape) | minutes | Large, and complementary — a mechanical low-pass ahead of the silicon is the only thing that helps with pre-ADC aliasing. |
| Balance the prop; mount the IMU near the pivot | minutes | Moderate. Centripetal error also shrinks: ~2.6° at r = 50 mm, 3 rad/s, versus ~15° at the tip. |
| Raise I²C to 400 kHz ([`i2c.c:65`](../BOARDS/NUCLEO-F401RE/Core/Src/i2c.c) is 100 kHz), sample 500–1000 Hz, decimate on the MCU | moderate | Moderate. Real anti-aliasing. Also cuts the blocking 14-byte read from ~1.5 ms to ~0.4 ms. |

**Recommendation:** fit the encoder *and* keep the IMU. Close the loop on the encoder so the
rig works, then run the IMU pipeline alongside and compare. The disagreement is the more
interesting result, and you are never blocked on filter tuning to make it fly.

---

## 6. Control feasibility: comfortably inside the envelope

```
J·θ̈ = L·T(u) − M_eff·g·L·sin θ − b·θ̇

ω_n = sqrt( M_eff · g · L · cos θ₀ / J )
```

For the representative build (`J ≈ 2.85e-3 kg·m²`, `M_eff = 35 g`, `L = 300 mm`):

| Quantity | Value | Notes |
|---|---:|---|
| Natural frequency, hanging | 6.0 rad/s · 0.96 Hz | Period ≈ 1.05 s |
| Natural frequency at θ₀ = 60° | 4.3 rad/s · 0.68 Hz | Softens with cos θ₀ — gain-schedule or feedforward |
| Open-loop damping ratio | ζ ≈ 0.02 – 0.05 | Bearing + prop drag only; rings 30+ cycles |
| Actuator lag (thrust ← throttle step) | ≈ 30 – 60 ms | **Binding constraint — measure it** |
| Achievable closed-loop bandwidth | ≈ 1 – 2 Hz | 1–2× ω_n; settling 2–3 s |
| Recommended control rate | 200 – 250 Hz | ~100× plant, ~4× actuator pole |

### Three things worth designing in from the start
1. **Gravity feedforward** — command `T_ff = M_eff · g · sin θ_ref` and let the PID handle
   only the error. Removes the steady-state bias the integrator would otherwise wind up.
2. **Invert the thrust curve** — command thrust, not throttle: `u = (T/T_max)^(1/1.58)`.
   Loop gain then stops varying ~2× across the range.
3. **Use the gyro as the derivative term** — you have a direct `θ̇`. Differentiating a noisy
   angle estimate is the classic way to make an aeropendulum buzz.

### Actuation resolution is not a constraint
At 84 MHz (APB1 timer clock), TIM3 with PSC = 20 gives a 4 MHz count — 4000 discrete steps
across the 1000–2000 µs pulse range. One step moves the arm by ~**0.04°**, three orders of
magnitude below sensor noise. **DShot is not needed.** Standard 1–2 ms PWM at 250 Hz is
plenty and BLHeli_S auto-detects it during arming. Keep DShot as a later exercise.

---

## 7. Firmware gap analysis

No timer is configured anywhere in the `.ioc`, and every peripheral access in `APPS/` is
blocking polled LL.

| Capability | Status | What is needed |
|---|---|---|
| IMU read, COBS framing, USART telemetry | **Done** | — |
| I²C bus recovery on boot | **Done** | Keep it; the MPU6050 does hang SDA across resets |
| Timer / PWM output | **Absent** | Add TIM3_CH1 on PA6 via CubeMX (leaves PA0/PA1 free for a current sensor or pot). 250 Hz, 1000–2000 µs |
| Deterministic timebase | **Absent** | Loop paced by `LL_mDelay(10)` plus blocking I²C/USART. That jitter goes straight into the derivative term. Move to a SysTick or TIM interrupt tick |
| Host → board command link | **Absent** | Telemetry is one-way. `cobs_decode()` already exists in [`cobs.c`](../APPS/src/cobs.c) — add USART2 RX and a command frame for setpoint, gains, arm/disarm |
| MPU6050 DLPF configuration | **Reset value** | One `MPU6050_WriteReg(0x1A, …)` call — see §5 |
| Arming / disarm state machine | **Absent** | BLHeli_S needs 1–2 s of valid minimum-throttle signal before it will arm. Wants explicit DISARMED → ARMING → ARMED → FAULT |
| Failsafe & watchdog | **Absent** | IWDG, angle-limit cutoff, host-link timeout, throttle slew limit. Non-negotiable before the first powered run |
| Control loop | **Absent** | The point of the project |
| Interrupts / DMA | **None used** | Not required at 250 Hz, but the blocking 1.5 ms I²C read is 37 % of a 4 ms budget; 400 kHz fixes it cheaply |

**Structural note.** The app-selector pattern (one `#define` in
[`app_selector.h`](../APPS/inc/app_selector.h), dispatched in `main_app.c`) has worked well for
standalone demos, but the aeropendulum needs the IMU driver, ESC driver, filter and
controller as *separately testable modules* rather than one more `*_loop()`. Worth splitting
`mpu6050_telemetry.c` into a reusable `mpu6050.c` driver first — the telemetry app then
becomes one thin consumer of it, and the control app another.

---

## 8. Missing from the parts bin

Everything in `.docs/` is powertrain. Two items below are the kind that quietly destroy an ESC.

| Item | Suggested | Why |
|---|---|---|
| **ESC bulk capacitor** | 220–470 µF, 35 V low-ESR across the ESC battery input | **Classic ESC killer.** BLHeli_S switching into inductive battery leads produces spikes that punch through the FETs. Check whether the LANRC shipped with one |
| **Prop guard** | Polycarbonate ring or mesh cage over the full disc | See §9. Not optional |
| Battery | 2S LiPo, 450–850 mAh, XT30 | Per §4. A LiPo also absorbs regenerative current from BLHeli_S damped-light braking, which a bench PSU cannot sink — a real way to trip OVP or damage the ESC |
| Pivot bearings | 2 × 623ZZ or 688ZZ on a ground shaft | Friction here is unmodelled damping and stiction — the main cause of limit-cycle hunting. Ball bearings, not a bolt in a hole |
| Arm | Carbon tube, 8–10 mm OD, 250–350 mm | A floppy arm adds a bending mode vibration will excite. Keep first bending mode well clear of 20 Hz |
| Angle encoder | AS5600 + diametric magnet on the pivot shaft | Strongly recommended, per §5 |
| Base | Clamped to a bench, or ≥ 2 kg | Steady reaction moment is small; the impulse when the controller misbehaves is not |
| Mechanical hard stops | Foam-faced, at travel limits | Bounds the failure mode when the loop diverges — during tuning, it will |
| Battery-line kill switch | Inline, within arm's reach | Independent of firmware. The one thing that always works |
| Anti-spark lead, LiPo bag, eye protection | — | Standard practice |

### Two wiring details worth getting right the first time
- **Assume no BEC.** Power the Nucleo from USB (needed for telemetry anyway) and tie a
  Nucleo GND pin to the ESC signal ground. Star-ground at the ESC so motor current never
  shares a return path with the signal wire.
- **Mind the hinge crossing.** Put the ESC on the arm *near the pivot*: then only two battery
  wires and one signal wire cross the joint, instead of three stiff phase wires whose spring
  torque appears as a mystery disturbance in step responses. A service loop is fine for ±90°;
  no slip ring needed.

---

## 9. Safety

**Prop tip speed.** At the recommended 2S operating point (~10 000 RPM) the tips of a 3.5″
prop move at **47 m/s — about 170 km/h**. At 4S full throttle it is over 500 km/h.
Carbon-reinforced nylon at those speeds does not bruise; it cuts, and a shed blade becomes a
projectile. Choosing 2S cuts the energy substantially but does not make the rig safe to touch.

- **Guard the disc before the first spin-up**, not after the first surprise.
- **Eye protection every time the battery is connected.** Press-fit props on a 1.5 mm shaft
  can and do walk off.
- **Bench-test thrust before the motor goes on the arm** — clamped, guarded, on a kitchen
  scale. This also produces the real numbers for §4 and the actuator time constant for §6.
- **Firmware interlocks:** boot disarmed; explicit arm command; ramp-limited throttle; cut on
  angle limit, stale sensor data, and host-link timeout; IWDG on the control loop.
- **Never leave it armed unattended**, and keep the kill switch in hand during early tuning.
  PID tuning *will* produce divergence.

---

## 10. Suggested build order

| Stage | Work | Done when |
|---|---|---|
| **A · Characterise** | Clamp and guard the motor, run a throttle sweep on a scale with a 2S pack, log thrust and current (and RPM if possible). Step the throttle and time the thrust response | You have your own thrust curve and actuator time constant, replacing every estimate in §4 and §6 |
| **B · Actuate** | Add TIM3_CH1 PWM in CubeMX, write `esc.c` including the arming sequence, add an `APP_ESC_TEST` app driving throttle from a host command over USART RX | You can command the motor from Python and it responds smoothly and repeatably |
| **C · Mechanics** | Arm, pivot bearings, base, guard, hard stops, ESC at the pivot. Weigh everything and recompute `M_eff` | The arm swings freely and rings for many cycles, confirming low friction and the predicted ~1 Hz |
| **D · Sense** | Fit the AS5600 on the pivot. Set the MPU6050 DLPF, soft-mount the IMU near the pivot, move to a fixed-rate control tick | Angle stays clean with the prop at hover throttle — and you can quantify how badly the raw IMU estimate degrades |
| **E · Close the loop** | Gravity feedforward + thrust-curve inversion + PD on encoder angle and gyro rate. Add I last, only if there is residual offset | A step to a 30° setpoint settles in a few seconds without hunting |
| **F · Learn from it** | System-identify the plant from the step data, compare to the §6 model, then try LQR or a state observer on the same rig. Revisit the IMU-only estimate | The control-theory half of the project pays out |

---

### Bottom line

**Feasible with the parts already owned.** Buy a 2S pack rather than a 4S one, add an AS5600
and a bulk capacitor, build a guard, and the remaining work is firmware and control design —
the part the project set out to teach. The most valuable next step costs nothing: clamp the
motor down and measure its real thrust curve, because every number here that leans on the
manufacturer's table is standing on a different propeller.

---

*Sources: manufacturer data in `.docs/` (SpeedyBee 1404 V3 thrust table measured on a GemFan
3016R, HQProp T3.5×2×3, LANRC 45 A), the repository firmware as of `acea59e`, and standard
momentum-theory and rigid-body scaling. Mass figures, actuator lag and vibration amplitudes
are engineering estimates pending bench measurement — Stage A exists to replace them.*
