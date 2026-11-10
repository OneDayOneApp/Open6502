# NEMU/RTL DiffTest smoke test

This directory contains a minimal final-state co-simulation check:

1. Generate a small 6502 program image.
2. Run it in the NEMU `6502EMU` reference.
3. Run the same program in the SystemVerilog CPU with Icarus Verilog.
4. Compare final `A`, `X`, `Y`, `SP`, `P` and `PC`.

Run:

```sh
./sim/difftest/run.sh
```

Expected:

```text
NEMU: 6502 A:42 X:04 Y:00 SP:FD P:24 ... PC:8008 ...
RTL : RTL A=42 X=04 Y=00 SP=FD P=24 PC=8008
PASS: NEMU/RTL final-state DiffTest 42 04 00 FD 24 8008
```

This is a final-state smoke test, not a cycle-level online DiffTest. It does not
yet compare bus transactions, PPU/APU/mapper state or interrupt timing.
