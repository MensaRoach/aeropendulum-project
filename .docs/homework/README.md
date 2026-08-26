# Homework

Step-by-step assignments for building this project's firmware from scratch.

The driver sources in `DRIVERS/` are written as a finished library: complete headers, documented API,
empty function bodies. **The headers are the specification.** Each assignment says which bodies to
fill in, in what order, and how to know when each one is right.

Nothing here contains answers you are meant to derive yourself. Where a value comes from a datasheet,
the assignment names the document and the chapter, not the number.

---

## The road

Each assignment ends with something that visibly works, and unlocks the next one.

| # | Assignment | You end up able to | Hardware needed | Status |
|---|---|---|---|---|
| **01** | [I2C bus driver & MPU-6050 library](homework_01.html) | Print to your laptop, read live gravity, and cross-flash with a partner | Board + sensor + partner's board | **Ready** |
| **02** | Non-blocking acquisition | Read the sensor without the CPU waiting around for it | Board + sensor | Planned |
| **03** | Timers & PWM generation | Produce a precisely-timed pulse train, and prove it is precise | **Board only** | Planned |
| **04** | ESC driver & safety | Arm a motor, command it, and make it fail safely | Board + ESC + motor + rig | Planned |
| **05** | Closing the loop | Hold the pendulum at an angle you choose | Everything | Planned |

```
01 ──► 02 ──┐
            ├──► 05
03 ──► 04 ──┘
```

01→02 and 03→04 are two independent tracks. They only need each other at 05, so if one gets blocked —
waiting on a part, waiting on a bench — the other still moves.

---

## Why timers and the ESC are two assignments, not one

It is tempting to treat "make the motor spin" as a single job. It is two, and they are different
kinds of problem:

**03 is a measurement problem.** Generate a pulse of a commanded width, then prove the pulse is
actually that wide. Everything hard about it lives inside the MCU: the clock path from the system
clock to the timer counter, how prescaler and auto-reload and compare together define a waveform, why
shadow registers exist and what tears if you write a compare value at the wrong moment. You verify it
with a scope, or with a second timer capturing the first.

**04 is a safety problem.** The signalling protocol, what the ESC expects at power-up, and — mostly —
a state machine: what state does it boot into, what does arming require, what happens on an MCU reset
while the ESC stays powered, what happens if the loop feeding it stalls, how fast may throttle change,
and what forces a disarm. Nearly all of that is reasoning about failure, not about registers.

Three reasons the split is worth it:

1. **03 needs no motor, no ESC, no battery, no stand.** Just the board you already have. You can
   finish it on a desk, at any hour, with nothing that can hurt you.
2. **A timing bug and a protocol bug look identical from the outside** — the motor beeps at you and
   does not spin. If 03 is finished and measured, you enter 04 already knowing your pulse widths are
   correct, so every remaining suspect is protocol or state.
3. **04 has a safety gate in front of it** — clamped stand, prop guard, kill switch in the battery
   line. Bundling it with 03 would mean that gate blocks the timer work too, for no reason.

---

## How these fit the rest of the docs

- [driver_development_plan.md](../driver_development_plan.md) — the *why* and the ordering.
  Assignments map onto its milestone IDs: 01 covers `I0`–`I2`, `I4`, and the cross-flash integration test, 02 covers `I1b` and `I3`,
  03 covers `E0`–`E1`, 04 covers `E2`–`E4`.
- [driver_development_log.md](../driver_development_log.md) — the decision record. Every assignment
  asks you to add entries; that is what makes a later review feedback on the *thinking*.
- [DRIVERS/README.md](../../DRIVERS/README.md) — the layering rule the code follows.

---

## Ground rules

1. **The headers do not change.** If a signature looks wrong, say so and discuss it — do not quietly
   edit it. The API was designed before the implementation on purpose, and finding a genuine flaw in
   it is a better outcome than working around one.
2. **Never return `DRV_OK` from something that did not happen.** A function reporting success it did
   not achieve is worse than one that fails, because the caller carries on regardless.
3. **No unbounded waits.** Every `while` polling a hardware flag needs a way out.
4. **Commit per task.** One commit per numbered task makes the reasoning reviewable, not just the
   final diff.
5. **Build it yourself.** There is older firmware in this repo's git history that solves some of these
   problems. Reading it skips the part with the most transferable value — turning a reference manual
   into working code — so it is off-limits, and the assignments are written assuming you have not seen
   it.

---

## Workflow

To make the collaboration model work without chaotic merge conflicts, follow this cadence:

1. **Branch naming**: Use descriptive, scoped branch names (e.g., `feat/hw01-i2c`, `fix/uart-timeout`).
2. **Commit per task**: Make a commit for every numbered task (e.g., A1, A2) in the assignment. Your commit message should explain what was done and *why*.
3. **Merge cadence**: Merge with your partner when a milestone or assignment is complete. Do not merge in the middle of an assignment unless explicitly instructed.
4. **Synthesis sessions**: Do not blindly resolve git conflicts. Sit together, run `git diff a..b -- path` or `git checkout --conflict=diff3`, and treat the merge as a discussion to synthesize the best implementation choices from both sides. You can use `git merge --no-commit` to pause and inspect the merge before finalizing it.
