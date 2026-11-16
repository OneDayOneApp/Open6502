**English** | [简体中文](README.zh-CN.md)

# am6502: a small Abstract Machine for 6502/NES

**Status: M1–M3 work.** Flat images run on the golden reference; the same
cartridge image runs on NEMU, GEM6502 and the RTL SoC; and `make difftest`
compares NEMU and RTL instruction by instruction, naming the first divergence.
`make run` prints `hello from am6502` and exits 0, `RUN_KLAUS=1 make test`
passes Klaus **with the reset vector the build produced**, and `hello.nes`
renders the same white frame on all three cartridge backends.

This is the 6502 subset of [nexus-am](https://github.com/OpenXiangShan/nexus-am)'s
idea: one abstraction to link against, one `make ARCH=<isa>-<platform>` matrix,
one image convention that every backend consumes.

```sh
cd labs/am6502
make                               # flat image for mos6502-nemu
make run                           # 6502EMU -a 0 -r: PASS, exit 0
RUN_KLAUS=1 make test              # hello + Klaus (downloads unless KLAUS_BIN is set)

make ARCH=mos6502-nes              # iNES cartridge: hello-mos6502-nes.nes
make ARCH=mos6502-nes run DUT=nemu # NEMU NES mode
make ARCH=mos6502-nes run DUT=gem  # GEM6502
make ARCH=mos6502-nes run DUT=rtl  # iverilog + tb/open6502_am6502_tb.sv

make difftest                      # NEMU vs RTL, per retired instruction
```

Needs `ca65`/`ld65` (cc65), `python3`, a built `NEMU/build/6502EMU`, and for
the cartridge backends a built `GEM6502/gem6502` and `iverilog`/`vvp`.

## Why this exists

The repository had three load paths and no common contract:

| Path | Image | Loader |
|---|---|---|
| Klaus test | `klaus.hex`, reset vector patched by the bench | `tb/open6502_klaus_tb.sv` |
| Game frame | `/tmp/falling/*_prg.hex`, `*_chr.hex` | `tb/open6502_game_tb.sv` |
| DiffTest / unit | `sim/difftest/difftest.bin` | `6502EMU -b --load-addr=...` |

A lockstep DiffTest needs the *same image* on both sides; a CI gate needs *one
command per program*. `am6502` is the smallest thing that makes both possible
for 6502-sized programs. The M1 image is byte-for-byte reproducible (two builds,
same SHA-256), which is the precondition for a golden hash.

## The image contract

| ARCH | Image | Entry | Host output / exit | Backends |
|---|---|---|---|---|
| `mos6502-nemu` | flat 65,536-byte `.bin` + derived `.hex` | reset vector at `$FFFC`, written by the build | `$F001` / `$F002` (existing 6502EMU ports) | `6502EMU -a 0 -r` |
| `mos6502-nes` | iNES `.nes` (NROM) + derived `.prg.hex`/`.chr.hex` | reset vector in the last PRG bank | `$4018` / `$4019`, decoded by NEMU, GEM6502 and the RTL bench | `6502EMU`, `gem6502`, RTL `tb` |

Every app declares how success looks, so `make test` means the same thing for a
compiled program and for a prebuilt `.bin`:

| `CHECK` | Pass | Fail |
|---|---|---|
| `halt` (default) | host exit port written with 0 | non-zero, or instruction budget exhausted |
| `pc:0x3469` | PC reaches the address and stops | budget exhausted elsewhere |

## Layout

| Path | Purpose |
|---|---|
| `am/am.h`, `am/include/amdev.h` | the contract: TRM + IOE subset, `_DEV_*` ids |
| `am/arch/{mos6502-nemu,mos6502-nes}.mk` | platform: image kind, load address, sources |
| `am/src/nemu/{crt0.s,trm.s,flat.cfg}` | flat startup, host ports, ld65 layout |
| `am/src/nes/{crt0.s,trm.s,nes.cfg}` | cartridge startup, `$4018/$4019`, NROM layout |
| `apps/hello/` | smoke test: `_putc` + `_halt(0)`, plus a white PPU frame on NES |
| `apps/klaus/` | external functional test wrapped as an image (`CHECK=pc`) |
| `tools/mkimage.py` | pad to 64 KiB or wrap in iNES, verify vectors, emit hex |
| `tools/ines.py` | inspect/extract third-party `.nes` images |
| `tools/run.py` | DUT dispatch, `CHECK` predicates, RTL bench compile, failure diagnosis |
| `../../tb/open6502_am6502_tb.sv` | RTL bench: boots the cartridge, decodes `$4018/$4019` |
| `docs/design.md` | full specification, milestones, risks |

## What M1/M2 do and do not do

| Done | Not yet |
|---|---|
| `mos6502-nemu` and `mos6502-nes` platforms, crt0 + TRM | C toolchain (llvm-mos), `mainargs`, `_heap` |
| One image on NEMU, GEM6502 and RTL (cartridge), byte-reproducible | real IOE drivers (input, timer, PPU, cart) |
| `hello` and Klaus through `make run` / `make test` | `.elf`/`.sym` artifacts |
| `$4018`/`$4019` decoded by NEMU and GEM6502; bench decodes them on the bus | frame-hash parity between NEMU and RTL |
| `.hex`, `.map`, `.lst` artifacts | migrating `sim/game` to `tools/ines.py` |
| `make difftest`: per-instruction lockstep NEMU vs RTL | |
| CI gate: `.github/workflows/am6502.yml` | |

`_io_read`/`_io_write` return 0 rather than trapping — the design's "one app,
many platforms" rule — until the NES platform gives them devices to talk to.

Measured today (`make ARCH=mos6502-nes test DUT=...`): `hello.nes` halts with
code 0 and renders a full white frame on all three backends — NEMU 3 frames,
GEM6502 3 frames, RTL 4 frames (61,440 pixels each).

`make difftest` retires 1,216 instructions of `hello` on both NEMU and RTL
with an identical state trace (`PC A X Y SP P`, P masked to drop the B/U bits).
A divergence is reported as:

```text
FAIL: first divergence at instruction 100
  NEMU: PC=0426 A=00 X=0b Y=00 SP=ff P=24
  RTL : PC=04ff A=00 X=0b Y=00 SP=ff P=24
>>>    100 NEMU[...] RTL[...]
```

The same comparison works for Klaus with `make difftest APP=klaus` (~96M
instructions, roughly 1 GiB of trace).

## Read next

- [docs/design.md](docs/design.md) — the specification (English).
- [docs/design.zh-CN.md](docs/design.zh-CN.md) — 中文设计文档.
- [../../docs/open6502-chip/difftest.md](../../docs/open6502-chip/difftest.md) —
  what the DiffTest side of the contract has to satisfy.
- [../jit6502/README.md](../jit6502/README.md) — the sibling lab; same
  "standalone experiment" rules.
