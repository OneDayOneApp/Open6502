# NES/Famicom Must-Have Features and Status

Status:

- **DONE**: implemented with automated test/build evidence
- **PARTIAL**: implemented but timing/coverage/test is incomplete
- **MISSING**: not implemented

## 0. Necessity tiers

| Tier | Content | Required for a chip? |
| --- | --- | --- |
| L1 | CPU, bus, memory, video, audio, input, interrupts, DMA, clock/reset | yes, for any game SoC |
| L2 | 2A03, dot-level 2C02, sprite 0, DMC, T1 mappers, NTSC | yes, for full NES compatibility |
| L3 | T2/T3 mappers, Zapper, Four Score, PAL, NES2, expansion audio | no; depends on target software/region |
| L4 | DiffTest, hardware capture, trace, golden reference | not chip logic, but required to trust a reference |

Current v1.5 scope is "full NES compatibility + verification", not a minimal
chip feature set. A new SoC may drop L3/L4; a NES-compatible SoC needs L1+L2.

## A. CPU

| ID | Feature | Status | Evidence/gap |
| --- | --- | --- | --- |
| A1 | official 6502 instructions/addressing | DONE | Klaus PC=3469 |
| A2 | 2A03 no decimal mode | PARTIAL | decimal logic still present; profile not separate |
| A3 | instruction/bus cycles | PARTIAL | cycles exist; no per-bus trace |
| A4 | dummy read/RMW double write | PARTIAL | code behavior; no dedicated tests |
| A5 | NMI/IRQ sampling | PARTIAL | fields present; no cpu_interrupts_v2 |
| A6 | BRK/RTI/PHP/PLP flags | PARTIAL | Klaus coverage |
| A7 | OAM DMA stall/alignment | PARTIAL | 513-cycle approximation |
| A8 | DMC DMA stall | PARTIAL | DMC core exists; CPU stall missing |
| A9 | reset vector/state | DONE | REF reset vector |
| A10 | undocumented instruction profile | PARTIAL | partial legacy NOPs |

## B. Bus/input

| ID | Feature | Status | Evidence/gap |
| --- | --- | --- | --- |
| B1 | 2KB WRAM + mirrors | DONE | REF/frontend tests |
| B2 | PPU register mirrors | DONE | bus implementation |
| B3 | APU/IO map | PARTIAL | basic channels; DMC/IO incomplete |
| B4 | open bus | PARTIAL | field exists; scenarios untested |
| B5 | OAM DMA | PARTIAL | implementation; no cycle-level test |
| B6 | standard 1P/2P controllers | DONE | input/replay tests |
| B7 | Zapper | MISSING | |
| B8 | Four Score | MISSING | |
| B9 | Famicom expansion/microphone | MISSING | |
| B10 | input recording/replay | DONE | `NESINP01` format |

## C. PPU

| ID | Feature | Status | Evidence/gap |
| --- | --- | --- | --- |
| C1 | 341x262/312 dot timing | PARTIAL | dot loop + per-dot background; sprites scanline-composed |
| C2 | vblank/NMI exact dot | DONE | `tests/nes_ppu` |
| C3 | `$2002` clear/w latch | DONE | `tests/nes_ppu` |
| C4 | v/t/x/w scroll | PARTIAL | basic implementation; no dot tests |
| C5 | background fetch/shift | PARTIAL | per-dot pixels; no fetch/shift registers |
| C6 | sprite evaluation/8x16/priority | PARTIAL | scanline renderer |
| C7 | sprite 0 hit exact dot | PARTIAL | dot check + unit test; depends on scanline background |
| C8 | sprite overflow | PARTIAL | flag exists; not exact |
| C9 | OAM/`$2004`/OAM DMA | PARTIAL | implementation |
| C10 | `$2007` buffer/increment | PARTIAL | implementation; no open-bus tests |
| C11 | palette/mirror/open bus | PARTIAL | palette exists; open bus incomplete |
| C12 | nametable mirroring/four-screen | DONE | mapper state/tests |
| C13 | odd-frame skip | DONE | `tests/nes_ppu` |
| C14 | rendering enable effects | PARTIAL | basic behavior |
| C15 | frame/VRAM/OAM hash | DONE | REF/frontend parity |
| C16 | NMI/IRQ to CPU | DONE | REF frame loop |
| C17 | external PPU ROMs | MISSING | only self-tests |

## D. APU

| ID | Feature | Status | Evidence/gap |
| --- | --- | --- | --- |
| D1 | Pulse 1/2 | PARTIAL | implementation; no apu_test |
| D2 | Triangle | PARTIAL | no linear-counter edge tests |
| D3 | Noise | PARTIAL | no mode/period tests |
| D4 | DMC | PARTIAL | address/length/rate/loop/IRQ/output; no CPU DMA stall |
| D5 | frame sequencer 4/5-step | PARTIAL | sequence exists; no exact-cycle test |
| D6 | frame IRQ/`$4015` | PARTIAL | implementation; no test |
| D7 | nonlinear mixer | PARTIAL | approximate |
| D8 | 44.1kHz output | DONE | WAV/hash |
| D9 | audio hash/tolerance | DONE | REF/frontend parity |
| D10 | channel state export | PARTIAL | hash only |
| D11 | apu_test/DMC ROMs | PARTIAL | self-test only |

## E. Cartridge/mappers

| ID | Feature | Status | Evidence/gap |
| --- | --- | --- | --- |
| E1 | iNES | DONE | REF/frontend |
| E2 | NES 2.0 | MISSING | |
| E3 | NROM | DONE | tests |
| E4 | MMC1 | DONE | bank tests |
| E5 | UxROM | DONE | bank tests |
| E6 | CNROM | DONE | CHR bank tests |
| E7 | MMC3 | PARTIAL | PRG/CHR/IRQ; no A12 exact timing |
| E8 | AxROM | DONE | bank tests |
| E9 | PRG/CHR/CHR RAM/PRG RAM | DONE | REF tests/state |
| E10 | battery save | MISSING | |
| E11 | T2 mappers/expansion audio | MISSING | |
| E12 | mapper IRQ timing | PARTIAL | MMC3 scanline API only |
| E13 | mapper matrix | DONE | this document |

## F. Platform/golden reference

| ID | Feature | Status | Evidence/gap |
| --- | --- | --- | --- |
| F1 | NTSC 2A03/2C02 | DONE | frontend/REF |
| F2 | PAL 2A07/2C07 | PARTIAL | REF clock only |
| F3 | revision profiles | MISSING | |
| F4 | REF CPU+PPU+APU+mapper frame loop | DONE | 60-frame parity |
| F5 | frame/audio hash alignment | DONE | `67667b1cbc4c1cf3` / `e78422afbcb92ac3` |
| F6 | full state hash | DONE | `nes_ref_machine_hash()` |
| F7 | frontend snapshot/restore | DONE | `--save-state/--load-state` |
| F8 | NES extended DiffTest | MISSING | CPU-only `.so` |
| F9 | PPU/APU/mapper DiffTest | MISSING | |
| F10 | hardware capture alignment | MISSING | |
| F11 | dot/bus trace | MISSING | |
| F12 | external ROM suite | MISSING | Klaus/open games/self-tests only |

## v1.5 required features

1. dot-level background/sprite pipeline
2. exact sprite 0 hit
3. APU DMC including DMA stall
4. NES extended DiffTest
5. hardware capture alignment
6. T2/T3 mappers, accessories, PAL

P0: DMC, dot-level PPU, sprite 0, extended DiffTest.
P1: hardware capture, PAL.
P2: T2 mappers, accessories, expansion audio.
