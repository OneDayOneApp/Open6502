# v0.2.0-dev: timing-platform development status

Baseline: published `gem6502-v0.1.0` (commit `74ff1a1`). Development branch:
`gem6502/timing-v0.2`. Requirements and coding guide remain normative.

## G0: partial, not frozen

Initial ADR-001 decisions:

- Retain the legacy interpreter as a separate execution path.
- Use a new C11 kernel with per-instance state and stable event owner/kind IDs.
- Order events by `(tick, delta, phase, sequence)` and update time before callbacks.
- `run_until` includes its boundary tick and closes it to later submissions.
- A callback may schedule a later phase at the same tick, or an earlier phase
  in the next delta; same/earlier phase in the current delta is rejected.
- Default maximum delta is chosen by the embedding platform, not hardcoded as
  a hardware property. Excess delta returns a diagnosed error to the producer.
- CPU/PPU master-clock ratios may be tested at 12/4. This does not freeze reset,
  pin sampling, DMA, IRQ or memory wait semantics for either chip profile.

Pending G0: independently sourced profile contracts, pin/phase mapping, IRQ
sampling, OAM/DMC arbitration, address-map validation and configuration schema.
No chip profile is certified or frozen by this slice.

## G1: experimental foundation, not complete

Implemented in `src/sim/kernel.c`, independent of legacy `GemEventQueue`:

- Stable ordering, schedule/cancel/reschedule and inclusive execution boundary.
- Stable handles across reset; stale handles cannot cancel new events.
- Per-instance handler registry; event records contain IDs and scalar payloads.
- Past-time, phase regression, delta-limit, counter overflow and reentrancy checks.
- Clock edge calculation with integer arithmetic and overflow detection.
- Deterministic short tests for scheduling, two clock ratios and multiple instances.

`make test-kernel` runs the new tests without SDL or legacy CPU/device code.
The initial queue uses O(n) sorted-array operations for correctness and is not
performance calibrated. Kernel structures are exposed for this experimental
slice; object lifecycle, serialization and opaque API boundaries are pending.
No timing CPU, timing ports, DMA arbitration or calibrated SoC exists yet.
Legacy `run`, `game` and `play` continue using the legacy instruction path.

Next: finish G0 profiles, then object/clock lifecycle and reset ownership for G1;
introduce asynchronous timing ports and a RAM/traffic-generator experiment for
G2. Do not claim G1 completion merely because kernel unit tests pass.
