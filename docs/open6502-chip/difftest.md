# Open6502 DiffTest Protocol

## 1. Goal

Compare RTL DUT and NEMU REF under identical inputs:

- CPU state and bus transactions
- PPU registers/scanlines/framebuffer
- APU channels/samples
- mapper banks/IRQ/PRG RAM
- final frame/audio/state hashes

## 2. Topology

```text
+-------------------+    DPI/shared memory   +-------------------+
| Verilator RTL DUT | <--------------------> | NEMU NES REF      |
| open6502_top      |                        | nes_ref_machine   |
+---------+---------+                        +---------+---------+
          |                                            |
          +-------------- DiffTest runner -------------+
                              |
                        mismatch report
```

The DUT is `open6502_top` plus `open6502_difftest_bridge`. The REF is the
`src/nes/ref` API, in-process or in a separate process.

## 3. RTL observation record

```systemverilog
typedef struct packed {
  logic        valid;
  logic [1:0]  kind;      // 0=cpu, 1=bus, 2=ppu, 3=apu
  logic [63:0] tick;
  logic [15:0] addr;
  logic [7:0]  data;
  logic        we;
  logic [7:0]  source;
} open6502_dbg_event;
```

High-frequency PPU/APU events use DPI/shared memory in simulation; FPGA exports
only summaries/hashes.

## 4. Sync points

| Granularity | RTL | REF | Compare |
| --- | --- | --- | --- |
| instruction | `cpu_retire` | `cpu_step` | PC/A/X/Y/SP/P/cycles |
| bus | `bus_event` | bus trace | addr/data/we/tick |
| scanline | `ppu_scanline_end` | PPU state | v/t/x/w/OAM/status |
| frame | `frame_end` | framebuffer | frame hash |
| audio | `aud_valid` | APU samples | sample hash/tolerance |
| mapper | `cart_event` | mapper state | bank/IRQ/PRG RAM |

## 5. Mismatch report

```text
sync_point:
tick:
cpu_cycle:
scanline/dot:
field:
dut:
ref:
last_bus_events:
last_ppu_events:
last_apu_events:
state_hash_dut:
state_hash_ref:
replay:
```

## 6. REF independence

- REF and DUT must not share the same implementation.
- REF must be calibrated with Klaus, external test ROMs and hardware captures.
- The current NEMU REF and frontend share PPU/APU code; it is provisional.
- Every run records REF commit/config/state version/ROM hash.

## 7. Modes

```text
cpu-only      Klaus/hello first
frame-only    per-frame hash
scanline      per-scanline state
audio         per-sample
full          bus + PPU + APU + mapper
```

## 8. Performance budget

- Verilator + REF >= 10^6 cycles/s.
- Stop at the first mismatch and dump a window.
- FPGA uses frame/hash-level comparison only; dot-level stays in simulation.
