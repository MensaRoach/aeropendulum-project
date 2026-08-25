# Homework

Step-by-step assignments for building this project's firmware drivers from scratch.

The driver source files in `DRIVERS/` are written as a finished library: complete headers, documented
API, empty function bodies. **The headers are the specification.** Each assignment below tells you
which bodies to fill in, in what order, and how to know when each one is right.

Nothing here contains answers you are meant to look up yourself. Where a value comes from a
datasheet, the assignment names the document and the chapter, not the number.

| # | Assignment | Covers | Status |
|---|---|---|---|
| 01 | [I2C bus driver and MPU-6050 library](homework_01_i2c_mpu6050.md) | `bus_i2c.c`, `mpu6050.c` — blocking path | Ready |
| 02 | Non-blocking acquisition | Interrupt + DMA path, dropped-sample detection | Not written yet |
| 03 | ESC driver | Timer PWM, arming, failsafe state machine | Not written yet |

## How these fit the rest of the docs

- [driver_development_plan.md](../driver_development_plan.md) — the *why*: what to figure out and in
  what order. Assignments map onto its milestone IDs (`I0`–`I4`).
- [driver_development_log.md](../driver_development_log.md) — the decision record. Add an entry
  whenever an assignment asks you to choose something.
- [DRIVERS/README.md](../../DRIVERS/README.md) — the layering rule the code follows.

## Ground rules

1. **The headers do not change.** If you think a signature is wrong, say so and discuss it — do not
   quietly edit it. The API was designed before the implementation on purpose.
2. **Never return `DRV_OK` from something that did not happen.** A function that reports success it
   did not achieve is worse than one that fails, because the caller carries on regardless.
3. **No unbounded waits.** Every `while` that polls a hardware flag needs a way out.
4. **Commit per task.** One commit per numbered task makes it possible to review the reasoning, not
   just the final diff.
