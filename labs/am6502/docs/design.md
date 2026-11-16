# am6502: an Abstract Machine for 6502/NES

**Status: M1–M3 implemented.** Flat images run on `NEMU/build/6502EMU`;
cartridges boot on NEMU, GEM6502 and the RTL bench with the same image and the
same verdict; `make difftest` compares NEMU and RTL per retired instruction and
names the first divergence. The rest of this document is still the target, not
current behaviour; see README for the implemented/deferred split.

Last updated: 2026-11-16.

---

## 1. The problem this lab solves

Open6502 already has a golden reference (`NEMU/build/6502EMU`), a cycle model
(`GEM6502`) and an RTL SoC (`rtl/`). What it does **not** have is one way to get
a program into all three. Today there are three unrelated paths:

| Path | Producer | Image | Loader |
|---|---|---|---|
| Klaus functional test | external binary | `klaus.hex`, 64 KiB, reset vector overwritten to `$0400` by the testbench | `tb/open6502_klaus_tb.sv` (`$readmemh`) |
| Game frame | `sim/game/extract_ines.py` | `/tmp/falling/falling_prg.hex` + `falling_chr.hex` | `tb/open6502_game_tb.sv` (`$readmemh`) |
| Unit / DiffTest | hand-written bytes in a shell script | `sim/difftest/difftest.bin` (+ `.hex`) | `6502EMU -b --load-addr=...` |

Each path also has its own idea of where code lives, where the entry point is
and what "PASS" means. That is fine for one-off tests and fatal for the two
inner-loop goals in [docs/learning-path.zh-CN.md](../../../docs/learning-path.zh-CN.md):
a lockstep DiffTest needs *the same image* on both sides, and a CI gate needs
*one command per program*.

`nexus-am` answers the same question for RISC-V/XiangShan with three ideas:

1. an abstraction (`am.h`) that a program links against instead of a machine;
2. an ARCH matrix (`make ARCH=<isa>-<platform>`) that compiles the same app for
   every machine;
3. an image convention (`.bin` memory image at `0x8000_0000`, optional flash
   image at `0x1000_0000`) that both the RTL simulator and NEMU consume.

This lab ports exactly those three ideas, not the code.

## 2. Prior art and the licence boundary

`nexus-am` (OpenXiangShan) publishes **no license file** — the GitHub API
reports no `license` for the repository. Nothing here may copy its source,
comments, build files or linker scripts. What this lab takes is the *interface
tradition*:

| nexus-am | am6502 | Copied? |
|---|---|---|
| `am.h`, `_putc`, `_halt`, `_io_read`/`_io_write` | same names, 6502 subset | names and signatures only, independently written |
| `make ARCH=riscv64-xs`, `AM_HOME` | `make ARCH=mos6502-nes`, `AM6502_HOME` | convention, no files |
| `.bin` memory image | flat 64 KiB image / iNES cartridge | convention |
| `mainargs` at build time | `_am_mainargs` string | concept only |
| `am/arch/*.mk` selects `am/src/<platform>` | identical shape | shape only |
| TRM / IOE / CTE / VME / MPE | TRM + IOE only (see §3) | API names only |

Everything shipped here is written for this repository. If the namespace
becomes confusing, `am6502` may be renamed (`nes-am`, `cart-am`); the API
follows.

## 3. Scope: TRM + IOE, and why the rest is absent

AM has five extensions. Three of them need hardware the 6502 does not have:

| AM extension | Needs | On 6502 |
|---|---|---|
| TRM (Turing machine: `_putc`, `_halt`, heap) | nothing | **in scope** |
| IOE (device registers) | memory-mapped devices | **in scope**, NES-shaped |
| CTE (events, context switch, interrupts as events) | privilege state, `_Context` save/restore | out of scope: no privilege state; NMI/IRQ are vectors, not contexts |
| VME (page tables, address spaces) | MMU | out of scope: 16-bit flat bus |
| MPE (multi-core, barriers, atomics) | hart IDs, atomics, CLINT | out of scope: one core |

This is a design invariant, not a schedule: **a feature that would require
6502 hardware that does not exist is rejected, not emulated.** A fake `_Context`
that saves 5 registers and calls it a thread switch would teach the wrong
lesson.

Consequence: `am6502` apps are single-threaded, flat-memory bare-metal
programs. That is all a NES ROM ever is.

## 4. Artifacts and invariants

Two image formats, because the repository already has two kinds of program.
This is the one place where the 6502 forces a divergence from AM's single
`.bin`.

### 4.1 Flat image — `build/<app>-mos6502-nemu.bin`

| Property | Value |
|---|---|
| Size | exactly 65,536 bytes |
| Layout | one flat 64 KiB RAM/ROM space; code wherever the linker put it |
| Entry | reset vector at `$FFFC`/`$FFFD` — **never patched by a harness** |
| Consumer | `6502EMU -b -a 0 -r image.bin` (raw mode; host I/O at `$F001-$F003`) |
| Typical apps | Klaus, nestest, ALU tests, anything that needs all 64 KiB |

This is the image AM calls a "memory image". Note the difference from today's
Klaus flow: the vector is produced by the build, so `tb/open6502_klaus_tb.sv`
can eventually stop overwriting `$FFFC`.

### 4.2 Cartridge image — `build/<app>-mos6502-nes.nes`

| Property | Value |
|---|---|
| Container | iNES: 16-byte header, then PRG, then CHR |
| PRG | 16 KiB or 32 KiB (NROM); larger only with a mapper |
| CHR | 8 KiB (CHR-ROM) or 0 (CHR-RAM apps) |
| Entry | reset vector at the end of the last PRG bank, CPU maps `$C000`/`$8000` |
| Consumers | `6502EMU game.nes`, `gem6502 game ...`, `tb/open6502_game_tb.sv` |
| Derived artifacts | `build/<app>-<arch>.prg.hex`, `.chr.hex` for `$readmemh` |

The `.hex` files are *derived*, never authored: `tools/ines.py` is the single
converter, replacing the hard-coded `/tmp/falling` paths and generalising
`sim/game/extract_ines.py`.

### 4.3 Debug artifacts

`build/<app>-<arch>.{map,lst}` are produced for every build (M1); `.elf`/`.sym`
arrive with the C toolchain. They are what a DiffTest failure report should
attach, and they make "which instruction is at `$C123`" questions cheap.

### 4.4 Invariants

| ID | Invariant | Checked by |
|---|---|---|
| I1 | every image ends in a real program: `_halt()` is reachable, no fall-through into `BRK` | `make test` (exit code) |
| I2 | flat image is exactly 65,536 bytes and its reset vector points inside it | `tools/mkimage.py --check` |
| I3 | NES images carry a valid header, PRG/CHR sizes are 8 KiB multiples, mapper byte matches `ARCH` | `tools/ines.py --check` |
| I4 | building twice produces identical bytes (no timestamps, no `$RANDOM`, sorted symbols in `.sym`) | CI double build + `sha256sum` |
| I5 | the same `.nes` runs unmodified on NEMU, GEM6502 and RTL | `make test` matrix |

I4 is the quiet one that matters: a golden hash is worthless if the image that
produced it is not reproducible.

## 5. Platforms

`ARCH` is `<isa>-<platform>`, exactly like AM. The platform directory owns the
startup code, linker script and device drivers; the app only sees `am.h`.

| ARCH | platform source | Machine model | Image | Host I/O | DUTs |
|---|---|---|---|---|---|
| `mos6502-nemu` | `am/src/nemu/` | flat 64 KiB, no PPU/APU | flat `.bin` | `$F001` putc, `$F002` exit, `$F003` stdin (existing 6502EMU ports) | `6502EMU -a 0 -r` |
| `mos6502-nes` | `am/src/nes/` | 2 KiB WRAM, PPU `$2000-$2007`, APU `$4000-$4017`, joypad `$4016/17`, PRG `$8000+` | iNES `.nes` (NROM/UxROM/CNROM/AxROM) | `$4018` putc, `$4019` exit (new, host-side only) | `6502EMU`, `gem6502`, RTL `tb` |
| `mos65c02-nemu` (stretch) | reuses `nemu/` | flat 64 KiB + 65C02 opcodes | flat `.bin` | same | `6502EMU -C -r` |

Memory maps (`mos6502-nes`):

```text
$0000-$00EF  app zero page (toolchain allocation)
$00F0-$00FF  am6502 runtime zero page (see §6.2)
$0100-$01FF  hardware stack
$0200-$07FF  WRAM: .data/.bss, OAM shadow, software stack, heap
$2000-$2007  PPU registers       (_DEV_PPU)
$4000-$4017  APU registers       (_DEV_APU)
$4018        am6502 host putc    (write)
$4019        am6502 host exit    (write, then spin)
$8000-$FFFF  cartridge PRG space (NROM: 32 KiB or two 16 KiB banks)
```

`$4018-$401F` are the Famicom test registers; writes are ignored by real
hardware, so a `_putc` there is safe in a real cartridge and observable in
every emulator we control. This is the 6502 equivalent of AM's UART: not a
device the ISA defines, but a convention between the image and its host.

Memory map (`mos6502-nemu`): the program owns the whole 64 KiB; `$F001-$F003`
are the same host I/O ports `NEMU/6502EMU.md` already documents. **No new NEMU
feature is required for this platform.** That is deliberate: M1 can land
without touching a single line of C.

## 6. ABI

### 6.1 Entry state

The reset vector jumps to `_start` in the platform's `crt0` (assembly). On
entry the CPU is in a hardware-defined state: `SP` unspecified, `I` set,
decimal mode unspecified. `crt0` therefore:

1. sets `SP = $FF`, `CLD`, `SEI` (the only three things a 6502 boot can rely on);
2. clears the runtime zero page (`$F0-$FF`) and the platform RAM area
   (`$0200-$03FF` on `mos6502-nemu`);
3. calls `am_init()` (a real symbol, a no-op on the flat machine; the nes
   platform puts PPU/APU setup there);
4. calls `main()`;
5. calls `_halt(main())`.

`main` is `int main(void)` in v0. `argc/argv` is deliberately **not**
implemented: a 6502 C runtime with 2 KiB of WRAM cannot afford AM's
stack-built `argv`, and no current app needs it. Instead:

```c
const char *am_mainargs(void);   /* never NULL; "" when unset */
```

`make ARCH=... mainargs='m2'` bakes the string into `.rodata` as `_am_mainargs`,
mirroring AM's `-DMAINARGS=` mechanism without its `argv` parsing. M1 ships the
function but not the build variable: it always returns `""`.

### 6.2 Runtime zero page (`$F0-$FF`)

| Address | Name | Use |
|---|---|---|
| `$F0` | `_am_exit_req` | set to 1 by `_halt` before it writes the port |
| `$F1` | `_am_exit_code` | last `_halt` argument |
| `$F2/$F3` | `_am_frames` | 16-bit frame counter maintained by the NMI/RTI tick |
| `$F4/$F5` | `_am_ptr` | scratch pointer for `_io_*` helpers |
| `$F6-$FF` | reserved | must be zero at `_start`, apps must not touch |

Apps and toolchains get `$00-$EF`. `cc65`/`llvm-mos` default zero-page
allocations are configured (via linker config) to stay inside `$00-$EF`.

### 6.3 Stack and heap

- Hardware stack `$0100-$01FF`: pushed by `JSR`/interrupts, 256 bytes, nothing
  to configure.
- Software stack (C locals): placed by the linker config inside WRAM, below
  the heap; size stated in the `.map`.
- `_heap`: declared in `am.h`, not provided by any platform yet (M1 apps use no
  heap). The target is a linker symbol `_am_heap_start` through `$0800` on
  `mos6502-nes`; a `malloc` shim stays optional and is **not** part of M1.

### 6.4 Host ports

| Port | Direction | Platform | Semantics |
|---|---|---|---|
| `$F001` | W | nemu | write one character to the host log/console |
| `$F002` | W | nemu | exit the simulation with that byte as status |
| `$F003` | R | nemu | read one character (EOF stops the simulation) |
| `$4018` | W | nes | write one character to the host |
| `$4019` | W | nes | exit the simulation with that byte as status |

`_putc(c)` → `$F001` (nemu) or `$4018` (nes). `_halt(code)` → `$F002` (nemu) or
`$4019` (nes), then `SEI; JMP *` forever. On real hardware both writes land on
unused registers and the program simply spins; that is the correct behaviour for
a bare-metal `halt`.

**Implementation status (M2):** decoded in two hosts and observed in the bench:

1. `NEMU/src/nes/nes.c` — `nes_cpu_write()` turns `$4018` into a character and
   `$4019` into `NEMU_END` with that status.
2. `GEM6502/src/gem6502.c` + `main.c` — same ports, same halt semantics, process
   exit code follows the halt code.
3. `tb/open6502_am6502_tb.sv` — no RTL change needed: it watches
   `dut.u_cpu.bus_we && dut.u_cpu.bus_addr == 16'h4018/16'h4019`, prints the
   characters, and fails the run on a non-zero halt or a blank frame.

`rtl/bus/open6502_bus.sv` + `rtl/dbg/open6502_difftest_bridge.sv` still do not
decode the ports; that is only needed when an FPGA/debug probe wants them.

### 6.5 Exit status

Exit code is the low 7 bits of `_halt`'s argument (bit 7 is not distinguishable
from open bus on some hosts). `0` = pass, non-zero = fail with that code.
A run that reaches the instruction budget without a `_halt` is a **failure**,
not a pass: `make run` must report `TIMEOUT`, mirroring the Klaus harness.

### 6.6 Per-app success predicate

Not every app can call `_halt`. Klaus reports success by spinning at a fixed PC,
and it is an external binary this repository does not assemble. `apps/<app>/app.mk`
therefore declares one predicate:

| `CHECK` | Pass when | Fail when |
|---|---|---|
| `halt` (default) | host exit port written with 0 | exit port written non-zero, or instruction budget exhausted |
| `pc:<addr>` | PC reaches `<addr>` and stays there | any other PC when the budget runs out |

`tools/run.py` reads the predicate from the app, not from a shell variable, so a
`make test` line means the same thing for a compiled C program and for Klaus's
prebuilt `.bin`. Exhausting the budget is never a pass.

## 7. IOE device table

`am.h` keeps AM's shape:

```c
size_t _io_read (uint32_t dev, uintptr_t reg, void *buf, size_t size);
size_t _io_write(uint32_t dev, uintptr_t reg, const void *buf, size_t size);
```

`dev` numbers keep AM's magic constants (`am/amdev.h`), so a `_DEV_*` value means
the same thing here as in AM; the devices AM has no analogue for live in a
private `0x0000ac1x` range and are documented as an extension. Unimplemented
`(dev,reg)` returns 0 — never traps, never aborts, because the point is to let
one app build for several platforms.

| dev | Name | regs | Notes |
|---|---|---|---|
| `0x0000ac02` | `_DEV_INPUT` | 1 = 1P pad, 2 = 2P pad | bit layout = NES pad shift-register order (`A B Select Start Up Down Left Right`) |
| `0x0000ac03` | `_DEV_TIMER` | 1 = `UPTIME` (u32 frame counter) | `_am_frames`, incremented by the platform tick |
| `0x0000ac05` | `_DEV_SERIAL` | 2 = `SEND` (same effect as `_putc`), 1 = `RECV` (returns 0) | the serial device is the host console; `_putc` is not magic, it is `SERIAL_SEND` |
| `0x0000ac10` | `_DEV_PPU` (extension) | 1-8 = `$2000-$2007`, 9 = trigger OAM DMA | read/write pass-through; PPU quirks are the app's business |
| `0x0000ac11` | `_DEV_APU` (extension) | 1 = `$4000-$4017` window | non-NES platforms return 0 |
| `0x0000ac12` | `_DEV_CART` (extension) | 1 = mapper id, 2 = bank, 3 = mirroring, 4 = IRQ ack | lets a test ask "which build am I?" |

Deliberately absent: `_DEV_VIDEO` (the PPU *is* the framebuffer; a generic
framebuffer abstraction would hide the scanline behaviour this project is
trying to verify), `AM_DISK`, `AM_SDCARD`, `AM_AUDIO` (no host audio in the
inner loop).

## 8. Build and run interface

```sh
cd labs/am6502
make ARCH=mos6502-nemu APP=hello                # flat image
make ARCH=mos6502-nes  APP=hello                # cartridge image + .hex
make ARCH=mos6502-nes  APP=hello run DUT=rtl    # same image through the RTL bench
make test                                       # the matrix
```

| Command | Produces / does |
|---|---|
| `make ARCH=... APP=...` | `build/<app>-<arch>.{bin or nes, elf, map, lst, sym}` |
| `make run ARCH=... APP=... DUT=nemu` | `6502EMU -b -a 0 -r` (flat) or `6502EMU` (nes); verdict from `_halt` or `CHECK=pc:...` |
| `make run ... DUT=gem` | `gem6502 game <rom> <frames> <ppm>` (nes only) |
| `make run ... DUT=rtl` | `iverilog`/`verilator` top from `tb/`, loads the derived hex, greps `$4018` |
| `make test` | builds and runs every app on every DUT the ARCH supports; fails when the app's `CHECK` predicate does not hold |
| `make clean` | removes `build/` only — never touches `NEMU/`, `GEM6502/` or `rtl/` |

Environment:

| Variable | Default | Meaning |
|---|---|---|
| `AM6502_HOME` | `labs/am6502` | root of the AM tree (AM's `AM_HOME`) |
| `OPEN6502_ROOT` | two levels up | where `NEMU/`, `GEM6502/`, `rtl/`, `tb/` live |
| `NEMU` | `$OPEN6502_ROOT/NEMU/build/6502EMU` | reference binary |
| `GEM6502` | `$OPEN6502_ROOT/GEM6502/gem6502` | cycle model binary |
| `MAPPER` | `0` | RTL `open6502_top` parameter for `DUT=rtl` |
| `MAX_INST` | per app | instruction budget; timeout is a failure |
| `VERBOSE` | `0` | keep per-run logs under `build/logs/` |

M1 implements `AM6502_HOME`, `OPEN6502_ROOT`, `NEMU` and the per-app
`APP_MAX_INST`; the remaining rows are targets, not current behaviour.

The flat image is always loaded with `-a 0`; 6502EMU's default `$8000` would
place a 64 KiB image at the wrong address. This is the only place where the
AM-style "the image is the machine" convention needs a flag.

Failure modes must name the stage, not just the program:

```text
FAIL hello-mos6502-nes / rtl: timeout after 20,000,000 cycles, last PC=$C0A2
  exit port: never written; first dev write: $2000=0x3f at cycle 812
```

## 9. Repository layout

```text
labs/am6502/
├── Makefile                  # top-level: apps x arches, `run`, `test`, `clean`
├── README.md / README.zh-CN.md
├── am/
│   ├── Makefile              # builds `libam.a` per ARCH (AM's am/Makefile shape)
│   ├── am.h                  # the contract: TRM + IOE subset
│   ├── include/amdev.h       # dev/reg ids, _DEV_INPUT bit layout
│   ├── arch/                 # one .mk per ARCH, lists AM_SRCS (AM's arch/*.mk shape)
│   │   ├── mos6502-nemu.mk
│   │   └── mos6502-nes.mk
│   └── src/
│       ├── common/           # string/mem helpers, printf-lite over _putc, am_mainargs
│       ├── nemu/             # crt0, $F001-$F003 drivers, flat linker script
│       └── nes/              # crt0, $4018/$4019, NROM linker script
├── apps/
│   ├── hello/                # prints a line and halts with 0 — the smoke test
│   ├── klaus/                # downloads the external binary, patches vectors, no compile
│   └── nestest/              # opcode trace test: the first app with real output volume
├── tools/
│   ├── ines.py               # .nes <-> prg/chr hex, header check (replaces hard-coded /tmp paths)
│   ├── mkimage.py            # flat image assembly, vector check, reproducible byte order
│   ├── run.py                # DUT dispatch + failure formatting
│   ├── difftest.py           # NEMU + RTL trace run, compare, first-divergence report
│   └── difftrace.py          # trace comparator (P masked, length checks)
├── docs/
│   ├── design.md             # this file
│   └── design.zh-CN.md
└── .gitignore                # build/, obj/, logs/
```

`tools/ines.py` should absorb `sim/game/extract_ines.py` rather than duplicate
it; the RTL game bench then reads images produced by the same code path as
every other app.

## 10. DiffTest and CI hookup

This is the reason the lab exists. The contract with
[docs/open6502-chip/difftest.md](../../../docs/open6502-chip/difftest.md):

- **one image, two machines**: `build/hello-mos6502-nemu.bin` runs on
  `6502EMU -a 0 -r` with `NEMU_TRACE_STATE`; the same bytes, loaded unmodified
  into `tb/open6502_am6502_flat_tb.sv`, run on RTL with `+TRACE`. Both emit one
  line per retired instruction (`PC A X Y SP P`); `tools/difftrace.py` compares
  them with P masked to drop the B/U bits and prints the first divergence with
  context. `make difftest` wires the whole thing up.
- **one image, frame hashes**: `hello-mos6502-nes.nes` on `6502EMU --hash` vs
  RTL `tb` frame hash; mismatch prints the first differing frame, not "FAIL".
- **one seed**: `mainargs` is the only input knob in v0; no wall-clock, no
  uninitialised WRAM (crt0 zeros the runtime ZP and the RAM area, so `.bss` is
  zero before `main`).

CI is `.github/workflows/am6502.yml` (added with M3):

```text
make -C labs/am6502 test ARCH=mos6502-nemu        # no RTL, no iverilog needed
make -C labs/am6502 test ARCH=mos6502-nes DUT=nemu
make -C labs/am6502 test ARCH=mos6502-nes DUT=gem
make -C labs/am6502 test ARCH=mos6502-nes DUT=rtl # needs iverilog
make -C labs/am6502 difftest ARCH=mos6502-nemu    # per-instruction NEMU vs RTL
```

The first line is the interesting one: it can go green on a laptop with no
Verilog toolchain, which is what makes the gate survive contributors who only
touch C.

## 11. Milestones

| Milestone | Scope | Exit criteria | Rough effort |
|---|---|---|---|
| **M0** | this document + directory + index entries | reviewed, no code claimed | done with this commit |
| **M1** | `mos6502-nemu` end to end: `hello`, `klaus` wrapper, `tools/run.py` | `make test ARCH=mos6502-nemu` green on `6502EMU`; Klaus passes at `PC=$3469` **with the build's own reset vector** — **landed 2026-11-16** | 1-2 weeks |
| **M2** | `mos6502-nes`: iNES output, `ines.py`, `$4018/$4019` observation in tb, NEMU NES-mode decode | the same `hello.nes` runs on `6502EMU`, `gem6502` and RTL, frame non-blank, exit code 0 — **landed 2026-11-16** (3/3/4 frames, full white screen) | 2-3 weeks |
| **M3** | DiffTest + CI: retire-level compare, failure report, workflow | `make difftest APP=hello` prints the first differing instruction; CI runs the lines in §10 — **landed 2026-11-16** (1,216 instructions match; Klaus partial run matches 200k) | 2 weeks |
| stretch | `mos65c02-nemu`, `nes-mmc3` (mapper 4), `printf`/`malloc` shims, move `NEMU/nesgame` onto the same CRT | each independently useful; none blocks another | - |

M1 is deliberately NEMU-only: it needs no new RTL, no new emulator feature and
no new toolchain assumption beyond "we can assemble a 6502 program and patch a
vector". If M1 cannot go green quickly, the rest of the design is suspect.

## 12. Risks and open questions

| # | Risk / question | Mitigation | Needs a spike? |
|---|---|---|---|
| R1 | **Toolchain**: llvm-mos (`mos-nes-nrom`) vs ca65. The repo already uses `ca65/ld65` in `NEMU/nesgame/build.sh`; llvm-mos gives C but a heavier install | v0 ships assembly apps only (`hello.s`, `nestest`, Klaus wrapper); C support is an upgrade, not a dependency | yes — 1 day: build `hello.c` with llvm-mos, measure PRG size |
| R2 | 32 KiB PRG / 2 KiB WRAM is tight; a C CRT may eat the budget | keep the CRT minimal, measure `.map` in CI, publish the numbers next to `jit6502`'s benchmark table | no — measured in M1/M2 |
| R3 | NES-mode `$4018` decode in NEMU is a change to the golden reference | it is host-only, behind a flag, and does not alter CPU/PPU timing; alternatively observe only in the tb until M3 | no |
| R4 | RTL top currently instantiates mapper 0 from the bench | parameter `MAPPER` already exists; UxROM/CNROM/AxROM need a bench parameter, not RTL work | no |
| R5 | nexus-am has no license | nothing copied; this document records the boundary (§2) | no |
| R6 | Naming: "am6502" may imply AM compatibility | README states "interface-compatible in spirit, not a port"; rename is cheap | no |
| R7 | Does this duplicate `NEMU/nesgame/build.sh`? | nesgame becomes one `apps/` entry (its `combine.py` folds into `tools/ines.py`); no second build system survives | no |

Open question worth a decision before M1: **should `_putc` on
`mos6502-nes` write to the PPU text console instead of `$4018`?** A console is
what makes nestest/blargg output readable on a *real* NES and in any emulator
without host-side patches, but it needs vblank sync and scroll preservation
(~150 lines and a new failure mode). Recommendation: `$4018` in v0, PPU console
as a `nes-console` platform in the stretch list.

## 13. Attribution

- The Abstract Machine interface (`_putc`, `_halt`, `_io_read`, `_io_write`,
  `ARCH=<isa>-<platform>`, `mainargs`, `AM_HOME`) is the design of the AM
  project (NJU PA / `nexus-am`, OpenXiangShan). This lab reimplements the
  subset a 6502 can support and copies no source; see §2.
- NES memory map, iNES container and mapper behaviour follow the public NES
  documentation already used by `NEMU/src/nes/` and `rtl/cart/`.
- Klaus, nestest and blargg test ROMs remain under their own licenses and are
  **downloaded, never vendored** — the same rule `roms/LICENSES` already
  applies to bundled ROMs.
