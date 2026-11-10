# 基于 NEMU 的完整 NES/Famicom Golden Reference 设计方案

## 1. 设计定位

Golden reference 不是一个脱离现有工程的新模拟器，而是 **NEMU 的一个专用参考构建**：

- 以 NEMU 的 CPU 执行引擎、ISA 框架、内存模型、监视器、trace 和 DiffTest 基础设施为底座；
- 在 NEMU 内扩展 NES/Famicom 平台：2A03 CPU/APU、2C02 PPU、总线、手柄和卡带 mapper；
- 生成的 REF 可执行文件/共享对象命名为 `nes-ref`；
- DUT 可以是 RTL、NEMU 的另一构建，或其他模拟器，通过 NEMU DiffTest 协议与 REF 对比；
- 不允许把当前功能型 `6502EMU` 前端直接当作 golden reference；它只保留为原型和
  smoke test 平台。

核心原则：

1. **NEMU 是 host/framework，NES 是 guest machine。**
2. **REF 与 DUT 不能是同一二进制、同一构建配置。**
3. **CPU 用 NEMU 的 ISA/DiffTest 机制；PPU/APU/mapper 通过 NES 扩展协议对比。**
4. **确定性由 NEMU 的 master tick 和状态序列化保证，不依赖 SDL/墙钟。**
5. **REF 的合法性来自实机采集 + 规范 + 独立测试，不来自自比较。**

## 2. “完整”的验收边界

| 层级 | 内容 | 默认是否进入 golden |
| --- | --- | --- |
| T0 | NTSC 2A03 + 2C02 + 总线 + 标准手柄 + NMI/IRQ/DMA | 是 |
| T1 | mapper 0/1/2/3/4/7 + PRG RAM + IRQ | 是 |
| T2 | MMC5、VRC、Namco 163、FME-7、Zapper、Four Score、扩展音频 | 独立验收 |
| T3 | PAL 2A07/2C07、revision、clone | 独立 profile |

本文中的“完整”默认指 **T0+T1 全部通过**；T2/T3 不能用一个笼统的“支持全部”承诺。

## 3. NEMU 构建配置

新增两个配置：

### 3.1 `m6502-nesref_defconfig`

REF 主程序，可直接跑 ROM、导出 frame/audio/state：

```text
CONFIG_ISA_m6502=y
CONFIG_NES=y
CONFIG_NES_REF=y
CONFIG_MODE_SYSTEM=y
CONFIG_TARGET_NATIVE_ELF=y
CONFIG_MSIZE=0x10000
CONFIG_MBASE=0x0
CONFIG_PC_RESET_OFFSET=0xFFFC
CONFIG_DIFFTEST=y
CONFIG_ITRACE=y
```

### 3.2 `m6502-nesref-so_defconfig`

REF 共享对象，供 DUT 通过 NEMU DiffTest 加载：

```text
CONFIG_ISA_m6502=y
CONFIG_NES=y
CONFIG_NES_REF=y
CONFIG_TARGET_SHARE=y
CONFIG_MSIZE=0x10000
CONFIG_MBASE=0x0
CONFIG_DIFFTEST=y
```

注意：`CONFIG_PC_RESET_OFFSET` 不能用于 NES 的真实复位向量；NES 复位由
`src/nes/ref/init.c` 从卡带 `$FFFC/$FFFD` 读取。REF 构建只复用 NEMU 的内存和
CPU 框架，不照搬 NEMU 的 raw image 默认入口。

### 3.3 建议的新增 Kconfig

```text
config NES
  bool "NES/Famicom machine"
  depends on ISA_m6502
  default n

config NES_REF
  bool "Build NES golden reference"
  depends on NES
  default n

config NES_DIFFTEST
  bool "NES extended DiffTest protocol"
  depends on NES && DIFFTEST
  default y

choice
  prompt "NES reference build"
  default NES_REF_EXEC
config NES_REF_EXEC
  bool "Standalone REF executable"
config NES_REF_SO
  bool "DiffTest REF shared object"
endchoice
```

## 4. 模块布局（在 NEMU 内）

```text
NEMU/
├── configs/
│   ├── m6502-nesref_defconfig
│   └── m6502-nesref-so_defconfig
├── include/
│   ├── nes/nes.h
│   └── nes/difftest.h
├── src/
│   ├── isa/m6502/              # CPU 2A03 profile
│   ├── nes/
│   │   ├── ref/                # REF 入口、状态、DiffTest 导出
│   │   ├── bus/                # 总线、open bus、DMA
│   │   ├── ppu/                # 2C02 逐 dot
│   │   ├── apu/                # 2A03 通道/frame/DMC
│   │   ├── cart/               # iNES/NES2/UNIF
│   │   ├── mapper/             # mapper 实现
│   │   ├── input/              # 手柄/附件
│   │   ├── trace/              # bus/PPU/APU trace
│   │   └── state/              # snapshot/hash
│   ├── cpu/                    # 复用 NEMU cpu-exec
│   ├── memory/                 # 复用 paddr/vaddr
│   ├── monitor/                # 扩展 SDB 命令
│   └── utils/                  # 复用 log/state/timer
└── tests/
    └── nes/
```

### 4.1 与 NEMU 的边界

| 能力 | NEMU 已有 | NES 扩展 |
| --- | --- | --- |
| CPU 执行 | `cpu-exec.c`、`isa_exec_once` | 2A03 周期/总线事件 |
| ISA | `src/isa/m6502` | 2A03 变体、DMA、非法指令 |
| 内存 | `paddr/vaddr` | NES 地址映射、open bus |
| 设备 | `device/` MMIO | PPU/APU/mapper 作为 NES 设备 |
| 监视器 | SDB | `nes` 命令、帧/音频/状态 dump |
| trace | ITRACE | NES bus/PPU/APU trace |
| DiffTest | CPU regs | NES state/frame/audio 扩展 |
| 时间 | timer | master tick、NTSC/PAL 分频 |

## 5. 时钟与执行模型

### 5.1 Master clock

在 `src/nes/ref/clock.c` 中维护：

```c
typedef struct {
  uint64_t master_tick;
  uint64_t cpu_cycle;
  uint64_t ppu_dot;
  uint32_t ppu_scanline;
  uint32_t ppu_cycle;
  uint64_t frame;
  uint32_t cpu_phase;
  uint32_t ppu_phase;
} nes_clock;
```

- NTSC：master 21.477272MHz，CPU /12，PPU /4。
- PAL：master 26.601712MHz，CPU /16，PPU /5。
- 用整数相位，不用浮点累加。

### 5.2 NEMU CPU 执行接入

保留 NEMU 的 `cpu_exec` / `isa_exec_once` 作为单指令入口，但 NES REF 额外提供：

```c
void nes_run_master_ticks(uint64_t n);
void nes_step_instruction(void);
void nes_step_frame(void);
```

每次 CPU 总线周期调用：

```c
void nes_bus_cycle(uint16_t addr, uint8_t data, bool write, uint8_t source);
```

该函数负责：

1. 推进 CPU/PPU/APU 时钟；
2. 记录 trace；
3. 更新 PPU/APU/mapper；
4. 处理 DMA/中断。

### 5.3 指令级与周期级

NEMU 原 `cpu_exec` 以“一条指令”为最小单位；NES REF 需要把单条指令拆成总线周期：

```text
fetch opcode
fetch operand(s)
dummy read / effective address
read / write / RMW double write
internal cycles
```

验收时必须与标准 NEMU 的 `cpu_exec` 结果一致，同时总线事件与硬件/参考一致。

## 6. 总线设计

`vaddr_ifetch/read/write` 在 NES 配置下路由：

```c
if (nes_active) {
  return nes_bus_read(addr, source);
}
return paddr_read(addr, len);
```

地址映射：

```text
$0000-$1FFF  2KB WRAM，4 次镜像
$2000-$3FFF  PPU 寄存器，8 字节镜像
$4000-$4017  APU、OAM DMA、手柄
$4018-$401F  test/expansion
$4020-$5FFF  expansion
$6000-$7FFF  PRG RAM
$8000-$FFFF  cartridge PRG
```

必须建模：

- open bus；
- PPU/APU 寄存器副作用；
- OAM DMA 的 halt/steal；
- DMC DMA 与 CPU 的总线竞争；
- mapper 对 CPU/PPU 地址的重映射。

## 7. CPU 2A03 Profile

在 `src/isa/m6502` 增加 2A03 profile，而不是复制 NEMU 的 6502 核：

- 关闭 decimal 模式；
- 加入 2A03 复位/标志行为；
- 官方指令 + 目标稳定非法指令；
- 每指令总线周期事件；
- NMI/IRQ 采样；
- OAM DMA / DMC DMA；
- 可选 `WAI/STP` 仅作为 profile 扩展，不属于 2A03。

CPU 状态：

```c
typedef struct {
  uint8_t a, x, y, sp, p;
  uint16_t pc;
  uint64_t cycles;
  bool nmi_pending, irq_pending;
  bool jammed;
  uint8_t opcode;
} m6502_cpu_state;
```

NEMU 的 `CPU_state` 仍是 `m6502_CPU_state`；新增字段只用于 REF 扩展状态，不破坏
现有 raw 6502/BASIC 模式。

## 8. PPU/APU/卡带作为 NEMU 模块

### 8.1 PPU

- 逐 dot（341 × 262/312）。
- 寄存器、v/t/x/w、OAM、nametable、palette、sprite hit/overflow。
- NMI 通过 `cpu.nmi_pending` 注入，IRQ 通过 `cpu.irq_pending`。
- framebuffer 作为 REF 输出；不直接依赖 SDL。

### 8.2 APU

- 脉冲 ×2、三角、噪声、DMC。
- frame sequencer、length/envelope/sweep、IRQ。
- 采样输出为 REF 的 side-band，不依赖 SDL 音频回调。

### 8.3 卡带

- 统一 mapper 接口；
- iNES/NES2.0；
- PRG/CHR bank、mirroring、PRG RAM、IRQ、扩展音频；
- mapper 0/1/2/3/4/7 为 T1 必过项。

卡带加载必须通过 REF 自己的入口，而不是复用
`init_monitor()` 的 raw image 加载：

```c
int nes_load_rom(const char *path);
int nes_load_rom_mem(const uint8_t *data, size_t size);
```

## 9. 状态、Trace、Hash

### 9.1 状态

在 NEMU 内新增 `src/nes/state`：

- CPU registers/cycles/pending；
- 2KB WRAM、PPU VRAM/OAM/palette、APU 通道、mapper、input；
- master tick、PPU dot、frame；
- RNG 和所有扩展状态。

NEMU 的 `save/load` SDB 命令调用 NES 状态序列化；状态格式带版本号。

### 9.2 Hash

- 运行期用 xxHash64/FNV 做快速对比。
- 归档用 SHA-256。
- 至少输出：CPU、PPU、APU、mapper、frame、audio、全系统 hash。

### 9.3 Trace

NEMU 的 ITRACE 继续记录指令；NES 增加独立 trace：

```text
master_tick cpu_cycle scanline dot bus_master rw addr data source
```

PPU/APU 事件 trace 可按 scanline/frame 采样，避免全 dot 日志过大。

## 9.4 NEMU 适配点（具体文件）

| 文件 | 需要改动 |
| --- | --- |
| `include/difftest-def.h` | m6502 `DIFFTEST_REG_SIZE` 必须与 `m6502_CPU_state` 实际布局一致 |
| `src/isa/m6502/reg.c` | 实现 `isa_difftest_checkregs`、寄存器 dump/copy |
| `src/cpu/difftest/ref.c` | 实现 m6502 `difftest_memcpy/regcpy/exec`；增加 NES 扩展导出 |
| `src/cpu/difftest/dut.c` | 增加 NES ROM/PPU/APU 同步入口 |
| `src/monitor/monitor.c` | NES REF 初始化，避免 raw image 路径误用 |
| `src/memory/vaddr.c` | `nes_active` 时路由到 NES 总线 |
| `src/isa/m6502/inst.c` | 2A03 profile、总线周期回调 |
| `src/nes/filelist.mk` | REF/DUT 源文件、TARGET_SHARE、SDL 分离 |
| `src/nes/ref/` | REF 入口、状态、hash、trace、DiffTest 导出 |
| `tests/nes/` | REF/DUT 回归脚本 |

### 9.5 第一个可验证里程碑

在实现完整 PPU/APU 之前，先完成：

```text
NEMU m6502-nesref 构建
  -> 加载 NROM
  -> CPU 跑 Klaus 测试
  -> CPU-only DiffTest（REF vs 另一构建）
  -> bus trace 与预期逐周期一致
```

这个里程碑通过后，再接入逐 dot PPU 和 APU，否则 PPU/APU 的差异会掩盖 CPU/总线
问题。

## 10. DiffTest 设计

### 10.1 CPU 部分复用 NEMU

- `difftest_regcpy` 对 CPU 寄存器。
- `difftest_exec(1)` 执行一条指令。
- DUT 侧 `isa_difftest_checkregs` 实现 m6502 对比。
- `difftest_memcpy` 用于 WRAM/VRAM/PRG/CHR 的同步。

### 10.2 NES 扩展协议

NEMU 的默认 DiffTest 只比较 CPU 寄存器，无法覆盖 PPU/APU/mapper。REF `.so`
额外导出：

```c
__EXPORT void nes_difftest_load_rom(const uint8_t *data, size_t size);
__EXPORT void nes_difftest_get_state(void *buf, size_t size);
__EXPORT uint64_t nes_difftest_state_hash(void);
__EXPORT uint64_t nes_difftest_frame_hash(void);
__EXPORT void nes_difftest_get_video(uint32_t *rgb, size_t pixels);
__EXPORT size_t nes_difftest_get_audio(int16_t *samples, size_t max);
__EXPORT uint64_t nes_difftest_get_master_tick(void);
__EXPORT void nes_difftest_step_frame(void);
```

DUT 侧新增 `nes_difftest_attach()`，负责：

1. 加载 ROM 到 REF；
2. 同步初始 WRAM/VRAM/CPU；
3. 按指令/scanline/frame 同步；
4. CPU 寄存器 mismatch 立即停；
5. PPU/APU/mapper mismatch 输出窗口。

### 10.3 同步粒度

| 粒度 | 比较 |
| --- | --- |
| instruction | CPU regs/cycles/pending |
| bus cycle | addr/data/rw/tick/source |
| scanline | PPU v/t/x/w、OAM、palette |
| frame | framebuffer/OAM/VRAM hash |
| audio block | samples hash/容差 |
| mapper | bank/IRQ/PRG RAM |

### 10.4 重要限制

NEMU 的 `init_difftest()` 现在把镜像复制到 `RESET_VECTOR`，这不适用于 NES
卡带。必须新增 NES 专用初始化路径，不能用 `--diff` 直接套用 raw image 逻辑。

## 11. 测试与验收

详细矩阵见 [test-plan.md](test-plan.md)，执行项见 [checklist.md](checklist.md)。

最低门槛：

- CPU：Klaus、nestest、instr_test-v5、timing、interrupts、DMA。
- PPU：vblank/NMI、sprite 0、overflow、open bus、palette、scroll。
- APU：apu_test、DMC、frame sequencer、mixer。
- mapper：0/1/2/3/4/7 bank/IRQ/save-state。
- 确定性：10^8 cycles hash 稳定，save/load 往返一致。
- DiffTest：REF 自比较零误报，人为错误可检出。
- 实机：关键事件在 tick 容差内。

## 12. 与当前 6502EMU 的关系

- `6502EMU` 保留为**平台前端和 smoke test**，不是 REF。
- `src/nes/` 当前代码作为 NES 模块原型，逐步替换为逐 dot/逐周期实现。
- `src/isa/m6502` 是 CPU 基础，但必须增加 2A03 profile 和总线周期。
- NEMU SDB 继续作为调试入口；REF 另提供 batch/headless 模式。
- 所有金标准判定在 `NEMU/src/nes` + NEMU DiffTest，不在 SDL 窗口里。

## 13. 实施阶段

```text
Phase 0  冻结 NEMU REF/DUT 配置、范围、状态/trace 格式
Phase 1  NEMU NES 总线、REF 入口、state/hash/trace、CI
Phase 2  2A03 CPU 周期级 + DMA + CPU DiffTest
Phase 3  2C02 PPU 逐 dot + frame hash
Phase 4  2A03 APU + DMC + audio hash
Phase 5  mapper 0/1/2/3/4/7 + save state
Phase 6  NES DiffTest 扩展 + RTL/外部 DUT 接入
Phase 7  实机采集对齐、兼容性回归、发布
```

## 14. 风险

| 风险 | 处理 |
| --- | --- |
| 把 NEMU 同一构建同时当 DUT/REF | 强制 REF/DUT 配置和构建分离 |
| NEMU raw image 加载逻辑污染 NES | NES 独立 REF 入口，不复用 `init_monitor` 的 load |
| DiffTest 只比较 CPU | 增加 NES state/frame/audio 扩展协议 |
| PPU/APU 逐周期性能 | trace 可关闭，快速路径与验证路径分离 |
| mapper/附件范围失控 | T0/T1/T2/T3 分层验收 |
| 参考模拟器互相不一致 | 实机采集优先，模拟器只作交叉检查 |
| 版权 | 只提交自由 ROM 和 hash/脚本，商业 ROM 由用户本地提供 |
