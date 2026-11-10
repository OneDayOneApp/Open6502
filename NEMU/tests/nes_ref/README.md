# NES REF M0 tests

This is the first test-driven slice of the NEMU-based NES golden reference.
It currently covers:

- deterministic NTSC/PAL master clock
- iNES/NROM parsing
- state hashing
- 2 KB RAM mirroring
- NROM PRG mapping and reset vector
- PRG RAM

Build and run:

```sh
./tests/nes_ref/run.sh
```

`tests/run.sh` runs this group before the SDL NES frontend tests.

It also has a 2A03 CPU backend ported from the NEMU m6502 core and an optional Klaus
functional test.  The next slice adds NEMU REF/DUT configs and CPU DiffTest.
