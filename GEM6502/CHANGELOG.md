# GEM6502 changelog

## 0.2.0-dev — in development

- Start a separate experimental event kernel with stable event identifiers,
  deterministic tick/delta/phase ordering and explicit execution boundaries.
- Add cancellation, rescheduling, reset isolation, clock edge arithmetic,
  overflow/reentrancy validation and `make test-kernel`.
- Record partial G0/G1 status and the initial kernel decisions.
- Keep the legacy instruction-driven player separate. Timing ports, timing CPU,
  calibrated chip profiles and full G1 lifecycle are not implemented yet.

Validation: kernel tests and ASan/UBSan pass; legacy unit/audio tests and build
pass. The v0.1.0 release received the full legacy/UI/differential/game suite.

## 0.1.0 — 2026-11-11

Preserved legacy emulator baseline: [release notes](releases/0.1.0.md).
