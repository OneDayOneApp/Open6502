**English** | [简体中文](README.zh-CN.md)

# am6502: a small Abstract Machine for 6502/NES

**Status: M1 works.** The `mos6502-nemu` platform builds, runs and verifies:
`make run` prints `hello from am6502` on the golden reference and exits 0, and
`RUN_KLAUS=1 make test` runs the Klaus functional test **with the reset vector
the build produced**, not one patched in by a bench.

This is the 6502 subset of [nexus-am](https://github.com/OpenXiangShan/nexus-am)'s
idea: one abstraction to link against, one `make ARCH=<isa>-<platform>` matrix,
one image convention that every backend consumes.

```sh
cd labs/am6502
make                      # build/hello-mos6502-nemu.bin (+ .hex, .map, .lst)
make run                  # 6502EMU -a 0 -r: PASS, exit 0
RUN_KLAUS=1 make test     # hello + Klaus (downloads ~64 KiB unless KLAUS_BIN is set)
```

Needs `ca65`/`ld65` (cc65), `python3`, and a built `NEMU/build/6502EMU`.

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
| `mos6502-nemu` (M1) | flat 65,536-byte `.bin` + derived `.hex` | reset vector at `$FFFC`, written by the build | `$F001` / `$F002` (existing 6502EMU ports) | `6502EMU -a 0 -r` |
| `mos6502-nes` (M2) | iNES `.nes` + derived `.prg.hex`/`.chr.hex` | reset vector in the last PRG bank | `$4018` / `$4019` (host-side only) | `6502EMU`, `gem6502`, RTL `tb` |

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
| `am/arch/mos6502-nemu.mk` | platform: image kind, load address, sources |
| `am/src/nemu/{crt0.s,trm.s,flat.cfg}` | startup, host ports, ld65 layout |
| `apps/hello/` | smoke test: `_putc` + `_halt(0)` |
| `apps/klaus/` | external functional test wrapped as an image (`CHECK=pc`) |
| `tools/mkimage.py` | pad to 64 KiB, fill/verify the reset vector, emit hex |
| `tools/run.py` | DUT dispatch, `CHECK` predicates, failure diagnosis |
| `docs/design.md` | full specification, milestones, risks |

## What M1 does and does not do

| Done | Not yet |
|---|---|
| `mos6502-nemu` platform, crt0 + TRM, reproducible images | `mos6502-nes` (iNES, `$4018/$4019`, PPU/APU) |
| `hello` and Klaus through `make run` / `make test` | C toolchain (llvm-mos), `mainargs`, `_heap` |
| `_putc`, `_halt`, `_io_read`/`_io_write` stubs, `am_mainargs` ("") | real IOE drivers (input, timer, PPU, cart) |
| `.hex`, `.map`, `.lst` artifacts | `.elf`/`.sym`, `DUT=gem`, `DUT=rtl`, lockstep DiffTest, CI |

`_io_read`/`_io_write` return 0 rather than trapping — the design's "one app,
many platforms" rule — until the NES platform gives them devices to talk to.

## Read next

- [docs/design.md](docs/design.md) — the specification (English).
- [docs/design.zh-CN.md](docs/design.zh-CN.md) — 中文设计文档.
- [../../docs/open6502-chip/difftest.md](../../docs/open6502-chip/difftest.md) —
  what the DiffTest side of the contract has to satisfy.
- [../jit6502/README.md](../jit6502/README.md) — the sibling lab; same
  "standalone experiment" rules.
