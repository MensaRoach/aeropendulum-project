# Driver development log

One entry per non-obvious decision, with the reasoning behind it. The point is that a later review
reads as feedback on the *thinking*, not just the syntax.

---

## 2026-08-25 — Setup

Added documents and plans to the `.docs` folder.

## 2026-08-25 — Phase 0 answers (first pass)

- Peripherals are owned by the drivers. If another device will use the same peripheral the main code
  will first call their init and deinit etc.
- The drivers will be flexible. We will start with blocking and then to nonblocking.
- The sensors will be struct/class objects. They will contain every function related to themselves.
  Whether peripherals editing their peripherals or doing scaling etc.
- Error surfacing: will be decided on the way.
- Everything will return fail or success.

## 2026-08-25 — Phase 0 answers revised, after scaffolding `DRIVERS/`

Three of the answers above did not survive contact with the code. Superseded, with the reasoning:

**Ownership — the bus is shared, not owned.** Init/de-init per device is expensive and racy, and it is
not how a shared bus works. Two devices on one I2C bus is the normal case, not the exception. The
revised model: one `i2c_bus_t` created once by application wiring; each device holds a transport
bound to it and never touches its lifecycle.

The peripheral-ownership line turned out to be already drawn by the toolchain: `LL_I2C_Init()` ends
with `LL_I2C_Disable()` and never re-enables, so **CubeMX owns configuration and the driver owns
enable and recovery.** That is the seam, and it was discoverable rather than a matter of taste.

**Error surfacing — decided now, not "on the way".** It appears in the type signature of every
function, so deferring it means rewriting every signature later. Replaced `bool` with `drv_status_t`.

**"Fail or success" is not enough.** A `bool` cannot distinguish *NACK — no device present* from *bus
wedged* from *timeout*, and those need different responses: re-probe, run bus recovery, retry. The
enum has one value per distinct recovery action; anything that does not imply a different response
does not earn a value.

**Blocking-first still stands**, but the API shape now makes the transition cheap: `read()` alongside
`start_read()` / `sample_ready()` / `get_sample()`, so moving to non-blocking is a call-site change
rather than a rewrite.

**Sensors as struct objects still stands**, with one correction: scaling is exposed as a *pure*
function (`mpu6050_convert`) separate from acquisition, rather than folded into the read. Raw counts
stay available, which keeps the existing telemetry wire format and the host tools working, and makes
the scaling testable off-target.

## Open decisions

- **I2C bus speed.** Still 100 kHz in the `.ioc`. The MPU6050 supports 400 kHz. Decide after
  measuring, not before.
- **Whether the async I2C path is needed at all.** Blocked on the same measurement. Requires
  `I2C1_EV_IRQn` / `I2C1_ER_IRQn` in CubeMX before it can even be attempted.
- **Whether calibration survives a reset**, and where it would be stored.
- **Whether to build the host command path now or defer it** (plan, cross-cutting practices).
