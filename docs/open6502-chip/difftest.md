# Open6502 DiffTest 协议

## 1. 目标

RTL DUT 与 NEMU REF 在受控输入下逐步运行，比较：

- CPU 架构状态与总线事务
- PPU 寄存器/扫描线/帧缓冲
- APU 通道/采样
- mapper bank/IRQ/PRG RAM
- 最终 frame/audio/state hash

## 2. 拓扑

```text
+-------------------+     DPI/共享内存     +-------------------+
| Verilator RTL DUT | <------------------> | NEMU NES REF      |
| open6502_top      |                      | nes_ref_machine   |
+---------+---------+                      +---------+---------+
          |                                          |
          +------------ DiffTest runner -------------+
                          |
                    mismatch report
```

- DUT：`open6502_top` + `open6502_difftest_bridge`
- REF：`src/nes/ref` API，运行在同一进程或独立进程
- 同步：主时钟 tick / CPU 指令 / scanline / frame 四层

## 3. RTL 观测接口

`open6502_difftest_bridge` 输出固定宽度记录：

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

PPU/APU 高频事件不应全部通过 UART；仿真时走 DPI/共享内存，FPGA 时只输出摘要/hash。

## 4. 同步点

| 粒度 | RTL | REF | 比较 |
| --- | --- | --- | --- |
| 指令 | `cpu_retire` | `cpu_step` | PC/A/X/Y/SP/P/cycles |
| 总线 | `bus_event` | bus trace | addr/data/we/tick |
| scanline | `ppu_scanline_end` | PPU state | v/t/x/w/OAM/status |
| frame | `frame_end` | framebuffer | frame hash |
| audio | `aud_valid` | APU samples | sample hash/容差 |
| mapper | `cart_event` | mapper state | bank/IRQ/PRG RAM |

## 5. 失配报告

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

## 6. REF 独立性

- REF 与 DUT 不能共享同一 RTL 实现。
- REF 必须由 Klaus、外部测试 ROM、实机采集校准。
- 当前 NEMU REF 与前端共享 PPU/APU 代码，只能作为 provisional REF。
- 每次 DiffTest 记录 REF commit/config/state version/ROM hash。

## 7. 运行模式

```text
cpu-only      先跑 Klaus/hello
frame-only    只比较每帧 hash
scanline      逐 scanline 状态
audio         逐采样
full          bus + PPU + APU + mapper
```

## 8. 性能预算

- Verilator + REF 目标 ≥ 10^6 cycles/s。
- 失败时暂停并导出窗口，不继续跑。
- FPGA 上只做 frame/hash 级对比，逐 dot 对比留在仿真。
