# AOT 能否加速模拟仿真？

**结论先行：可以，但"能加速多少"完全取决于加速的是哪一层。** 本仓库有三层彼此独立的
仿真栈，AOT（Ahead-of-Time，提前编译）在三层里的含义、收益和风险完全不同：

| 层面 | AOT 在这里指什么 | 本仓库现状 | 判断 | 实测/可引用收益 |
|---|---|---|---|---|
| **A. 功能级 6502/NES 模拟**（NEMU、GEM6502 legacy、`labs/jit6502`） | 把固定 ROM 的 6502 代码在加载/构建期翻译成宿主码，运行期不再取指译码 | 只有运行时 JIT（无 AOT） | **可行，但天花板低**：AOT 相对现有 chained JIT ≈ 0.9–1.3×，相对解释器 8–11× | CPU 负载 77 → 644 MIPS；NES 端到端 0.65 → 0.30 s/600 帧 |
| **B. RTL 仿真**（`rtl/` + `tb/`） | 把 RTL 编译成 C++ 模型（Verilator），替代解释型仿真器（Icarus） | **已经在用 AOT** | **AOT 就是答案，且这是唯一的瓶颈环节** | 单线程 ≈100× Icarus，多线程再 2–10× |
| **C. 时序/微架构平台**（GEM6502 v0.2 事件内核） | 把"事件驱动"编译成"静态调度"（Verilator 的核心思想），或按配置生成专用模型 | 排序数组事件队列 + 每事件间接回调 | **这件事才是高价值方向**，但它不是"翻译 6502 指令" | 待实测；结构性依据见 §5 |

一句话总结：**如果目标是"游戏能不能跑得更快"，AOT 不值得做——解释器已经比实机快 129×。
如果目标是"验证吞吐/时序仿真"，AOT 值得做，但在本仓库里该 AOT 的对象是 RTL 和时序内核，
不是 6502 指令流。**

---

## 1. 问题定义：先分清三种"AOT"

"用 AOT 加速模拟仿真"这句话在模拟器语境里至少对应三种不同的技术动作，混用会得出错误结论：

1. **静态二进制翻译（AOT of guest code）**：把客户机指令流翻译成宿主机器码/C，构建期或加载期完成。
   代表：[jamulator](https://github.com/andrewrk/jamulator)（NES→LLVM）、
   [N64Recomp](https://github.com/N64Recomp/N64Recomp)、[nesrecomp](https://github.com/mstan/nesrecomp)。
   对照物是**解释器**和**运行时 JIT（dynarec）**。
2. **编译型模型生成（AOT of the model）**：把硬件描述编译成宿主代码，即 Verilator 的
   "Verilate → C++ → 本地可执行文件"。对照物是**事件驱动解释型仿真器**（Icarus、Questa 的解释模式）。
3. **配置特化（AOT by specialization）**：把仿真配置/时序协议在编译期折叠成固定顺序的代码，
   消除运行期的调度、查表、间接调用。对照物是**通用事件队列内核**。

本仓库文献里没有出现过 AOT 这个词，`labs/jit6502` 只做到"运行时 JIT"。下面的分析全部基于
**本机今天的实测**（Apple Silicon，arm64，macOS 26.3.1，Homebrew clang 21.1.8，`-O2`），
复现命令见 §9。

---

## 2. 实测基线：钱花在哪里

### 2.1 纯 CPU 负载（`bench/arith_loop`，1 亿条 6502 指令，块=4 条指令）

| 引擎 | 时间 | 速度 | 周期率 | 相对 NES 实机 |
|---|---:|---:|---:|---:|
| 解释器 | 1.296 s | 77.2 MIPS | 231.4 MHz | 129× |
| JIT，不链接（`--no-chain`） | 0.314 s | 318.3 MIPS | 954.3 MHz | 533× |
| JIT，链接（`--chain`） | 0.155 s | 644.1 MIPS | 1931.1 MHz | 1079× |

`copy_loop` 同向更极端：235 MIPS → 796 MIPS（3.4×）。

**关键数字**：每个基础块 12.57 ns（不链接）vs 6.21 ns（链接）→ **"序言 + 收尾 + 返回派发器 +
PC→代码查表"合计约 6.4 ns（≈22 个宿主周期）/块**。这就是 AOT 在结构上能直接删掉的那部分成本。

### 2.2 NES 全机负载（`roms/falling.nes`，600 帧 = 10 秒游戏时间，17,868,188 个 6502 周期）

| 引擎 | 时间 | 帧率 | 说明 |
|---|---:|---:|---|
| 解释器 | 0.65 s | 923 fps | |
| JIT，不链接（默认，帧哈希精确） | 0.43 s | 1395 fps | 5,640,671 个块、599 次解释器回退 |
| JIT，链接（画面会撕裂，仅作上界参考） | 0.30 s | 2000 fps | 只剩 2187 个派发点 |

**关键数字**：不链接模式每块 76 ns，其中派发往返约 23 ns —— 也就是 **AOT 能删掉的那部分
占 NES 端到端时间的 30%，理论上限 1.43×**。其余 70% 是总线 helper（`$2000+` 读写）+ 每块一次的
`nes_sync()` + PPU 推进；这正是 `labs/jit6502/README.md` 里"78% 时间在 `ppu_step()`"那个 profile
结论的另一面。

### 2.3 结论性事实

- 解释器已经是 **129× NES 实机**；JIT 是 **~1400 fps（23× 于 60 fps 需求）**。
  **"能不能流畅跑游戏"早已不是问题，AOT 在这里没有产品价值。**
- DiffTest 预算要求 "Verilator + REF ≥ 10^6 cycles/s"（`docs/open6502-chip/difftest.md` §8），
  而 10^6 cycles/s 只有 NES 实机的 **0.56×**。REF 侧当前是 231×10^6 cycles/s，
  对 DUT 有 **200 倍以上余量**（§3.3 还有一组更保守的算法）。
  **加速 REF 不会让 DiffTest 变快，只会让 REF 更闲。**

---

## 3. 层面 A：功能级 6502/NES 模拟

### 3.1 AOT 在结构上能拿到什么

1. **零翻译/零派发**：目标地址在编译期已知，块尾直接是宿主 `b`/`bl` 或函数尾调用，
   不需要 `g_map[65536]` 查表、不需要 `ret` 回派发器。
2. **零预热、零代码缓存**：没有"第一次执行慢、跑了 N 秒才稳定"的问题；CI 里的
   短跑（200 帧回归）不再白付 JIT 预热成本。
3. **零失效逻辑**：ROM 内的代码不会被写，`--code` 窗口、页级 flush、整表 flush 这些
   机制在 AOT 里可以整个删掉（RAM 内代码走回退路径）。
4. **跨块优化**：循环旋转、跳转链折叠、常量传播（`LDA #$00 / TAX / TAY`）、
   把 `JSR/RTS` 变成真正的调用/内联、把 `JMP ($xxFF)` 的页错误语义在编译期定死。
5. **静态插入同步点**：这是 **AOT 相对本仓库现有 JIT 的真正差异点**，见 §3.4。
6. **可复现与可调试**：产物是 `.c`/`.s` + 符号表，PC→宿主地址可映射；
   trace 稳定，没有 JIT 的"块边界随编译时机变化"。

### 3.2 AOT 拿不到什么

**块链接（chaining）已经把"派发成本"消掉了**，而且是在运行期用 20 行代码做到的。
所以 AOT 的相对收益不是"解释器 → 机器码"的 8×，而是"chained JIT → 更好的代码生成"的
**0.9–1.3×**：本仓库 JIT 的寄存器约定已经把 A/X/Y/SP/P 放在 `w21..w25`、`mem+PC` 放在 `x26`，
AOT 能改进的主要是循环结构、常量折叠和内联，不是"把状态从内存搬进寄存器"。

**算术上界**（用实测数据外推，不是承诺）：

```
CPU 负载：  解释器 77.2 → chained 644 MIPS（8.3×，这就是 AOT 的可行区间 600–850 MIPS）
NES 全机：  解释器 0.65 s → 无派发上界 0.30 s（2.2×）；相对现有 JIT 只有 1.43×
端到端（含 PPU/APU/前端）：上限被 §2.2 的 70% 非 CPU 成本锁死
```

### 3.3 为什么这层"可行但不值得"

| 用途 | 现在的成本 | AOT 后 | 是否值得维护第二套代码生成器 |
|---|---:|---:|---|
| 玩一个游戏 | 已 1395 fps | ~2000 fps | 否，60 fps 已满足 23 倍 |
| Klaus 功能测试（3065 万条指令） | ~0.40 s | ~0.05 s | 否，CI 里可忽略 |
| 200 帧 × 6 ROM 帧回归 | ~1.5 s | ~1.0 s | 否 |
| ROM 模糊测试 10 万个 | 数小时 | 数小时/2 | 边界；先做并行（§8） |
| DiffTest 参考模型 | 有 200× 余量 | 余量更大 | 否 |
| 时序精确性（cycle-exact 对拍） | 需要"链接速度 + 精确同步" | **可能同时拿到** | **是，见 §3.4** |

### 3.4 唯一能让 AOT 在本仓库"赢"的场景：静态同步点

现在存在一个真实的取舍：**不链接 = 帧哈希精确但慢 1.43×；链接 = 快但画面撕裂**
（`labs/jit6502/README.md`：「with chaining the PPU only advances on I/O accesses… 出现横向黑条」）。
根因是 PPU 推进被绑在"块边界 + I/O 访问"上，而链式执行让块之间不再回到派发器。

AOT 可以把这件事从"运行期启发式"变成"编译期已知"：**在生成的代码里，按静态可计算的
周期预算插入 `nes_sync(cycle)` 调用点**（例如每 8 个可见 dot 一个同步点，或每条指令
累加已知周期数后条件同步）。这样得到的产物是：

- 直接分支、无派发（拿到 chained 的 1.43×）；
- 同步点位置由编译期决定，不依赖运行期是否回派发器（保住帧哈希精确）；
- 顺带把 `$2000–$4017` 的通用 bus helper 变成**按地址特化的内联代码**，这是 JIT 因为
  代码缓存成本而不愿意做的事。

这条路线不是"给 6502 加速"，而是"**用编译期信息换掉运行期的近似**"，是本仓库里 AOT 唯一
有独特价值的地方。收益估计 1.2–1.4×（相对不链接 JIT），但它同时买到"精确 + 快"，
而不是二选一。

---

## 4. 层面 B：RTL 仿真——AOT 已经是答案

`rtl/Makefile`、`sim/*/run.sh` 已经在 Verilator 和 Icarus 之间切换。这一层的结论不需要推导：

- Verilator 官方数据：单线程比解释型仿真器（Icarus）**快约 100×**，多线程再 **2–10×**
  （总 200–1000×）。[veripool.org/verilator](https://www.veripool.org/verilator/)
- 它之所以快，恰恰是因为**它不是翻译器而是编译器**：把 RTL 折叠成固定 `eval()` 顺序的
  C++ 代码，没有事件队列、没有解释循环、没有运行期名字查找。

所以"能不能用 AOT 加速仿真"在 RTL 这一层的答案是**已经用了，而且它就是唯一把
10^6 cycles/s 变成可能的工具**。剩余杠杆按性价比排序：

| 手段 | 预期 | 风险 |
|---|---|---|
| `--threads N`（多线程模型切分） | 2–10× | 需要设计可切分；调试变难 |
| 2-state 化（`--x-assign fast`、去掉 X 传播） | 1.3–2× | 丢 X 检查，需与 4-state 流程分离 |
| 关闭不必要功能（timing、trace、DPI、assert） | 1.2–3× | 不能用于 signoff 流程 |
| 把 DPI 事件改为共享内存环形缓冲 | 显著，取决于事件率 | 文档 §3 已提出，未实现 |
| 模型切分/只编译被测子系统 | 视设计而定 | 顶层集成后失效 |

注意：**这些优化不会因为"再写一个 AOT"而出现**，它们是 Verilator 参数与模型结构问题。

---

## 5. 层面 C：GEM6502 时序平台——这里"编译成静态调度"才是高价值 AOT

当前 `src/sim/kernel.c` 的结构是：有序数组事件队列 + `insert()/remove_at()` 的 `memmove`
+ 每个事件一次间接回调（`k->owners[event.owner].handler(...)`），按 `(tick, phase, sequence)`
排序。对 NES SoC 这种"CPU 和 PPU 每个周期都活跃"的负载，这是典型的**用解释器跑稠密图**：
每个 tick 都要做堆/数组维护、比较、间接调用。

Verilator 的做法——**把事件图在编译期折叠成固定顺序的 `eval()`**——在这里同样成立：

- 时钟域数量固定、phase 语义固定（0/10/20/40/60/70）、端口数量固定（≤4 initiator，
  1–8 slot，`spec/timing-bus.md`）→ 完全可以在编译期展开成一张静态调度表；
- 生成方式可以是"生成 C 源码 + `cc` 编译"（真 AOT），也可以是"生成函数指针数组 +
  静态 phase 顺序"（表驱动，改造成本最低）；
- 稀疏活动（长时间无事务）时静态调度会退化，所以正确形态是**混合**：静态 eval 承担
  每周期活跃部分，事件队列只处理真正稀疏的边缘事件。

这才是"用 AOT 加速模拟仿真"在本仓库最有价值、也最符合 `docs/synthesis-and-cost.md`
"用软件换验证吞吐"路线的一层。**前提是先有 G1 固定基准**（`GEM6502/docs/requirements.md`
已经禁止"未经实测的性能保证"）——所以顺序是：先量化 kernel 的每 tick 成本，再决定是否生成代码。

---

## 6. 如果真要做 6502 AOT：硬约束清单

这些是历史上 NES 静态重编译反复踩的坑，也是"为什么 AOT 在 6502 上比在 MIPS/N64 上更麻烦"
的原因：

| 约束 | 具体形态 | 本仓库的对应事实 |
|---|---|---|
| **代码发现不可判定** | 递归下降遇到 `RTS/RTI/BRK/JMP ($xxFF)` 就断；跳转表、`JSR` 间接、bank 内代码共享地址 | `labs/jit6502` 用"运行期回退解释器"绕开；AOT 必须同样保留回退 |
| **间接跳转目标** | `JMP ($xxxx)`、`RTS` 返回到运行时才知的地址 | 需 profile 引导：先跑一遍解释器收集目标集合（FX!32 式两遍法） |
| **Bank switching** | 同一逻辑地址在不同 bank 是不同代码；静态 PC→代码映射失效 | 本仓库 mapper 0/1/2/3/4/7；AOT 需 bank guard 或按 bank 分别生成 |
| **RAM 内代码/自修改** | 解压后跳 RAM 执行；测试 ROM 在 `$0200` 放 stub（Klaus 就是这样） | JIT 用"代码页失效"处理；AOT 只能回退解释器 |
| **周期精确细节** | dummy read、页跨越额外周期、`JMP ($xxFF)` 页 bug、DMA 513 周期、PPU dot 级同步 | JIT 已经因 DMA 周期丢过一次（README 记录 3 个 DiffTest 抓到的 bug） |
| **验证成本** | 每加一个代码生成后端，就要重跑 4 道门 × 全 ROM 集 | `make test` 已有：Klaus 指令数、定点状态、随机差分、6 ROM 帧哈希 |

外部证据也一致：

- [jamulator](https://github.com/andrewrk/jamulator)——用 LLVM 做 NES 静态重编译，
  **只支持 Super Mario Bros. 1，已停止维护**；
- [nesrecomp](https://github.com/mstan/nesrecomp)——NES 静态重编译框架，仍是 **beta**；
- [N64Recomp](https://github.com/N64Recomp/N64Recomp)——能成功，但**依赖反编译项目提供的
  符号与元数据**（ELF + 函数边界），不是纯二进制输入；
- [FX!32](https://web.stanford.edu/class/cs343/resources/fx32.pdf)——AOT 的经典成功案例，
  靠的是 **profile-directed**（先解释执行收集 profile，再翻译热点）；
- 反例：HP Dynamo 表明**运行期优化可以比 AOT 更快**（~20%+），因为 AOT 拿不到真实 profile。

对本仓库的直接含义：**不要做"通用 NES AOT"**。做"profile 引导的 AOT"，对象固定为
6 个 golden ROM + Klaus + 模糊测试语料，回退路径永远是解释器。

---

## 7. 建议的最小可行 spike（`labs/aot6502`，1–2 周量级）

如果 §3.4 的"静态同步点"值得验证，按下面做，失败也能在一周内止损：

```
两遍式：
  pass 1  解释器跑 N 帧，记录：被执行 PC 集合、间接跳转目标、被写的代码页、每 PC 的周期数
  pass 2  对 pass 1 的 PC 集合做递归下降 + 跳转表识别，emit C
  链接    clang -O2 -c，与 runtime 链接成独立可执行文件（不是 dlopen 也行，更接近 AOT 语义）
  运行    未覆盖 PC / bank 切换 / 代码页写入 → 回退 interp_step()，并记一条 "cold PC" 统计
```

设计取舍：

- **emit C 而不是 emit AArch64**：让 clang 做寄存器分配、内联、常量传播、循环优化；
  同时天然可移植到 x86-64 与 wasm（`GEM6502/web` 已经是 wasm 产物）。AArch64 直出只适合
  教学（`labs/jit6502` 的价值就在"看得见每条指令"）。
- **与 JIT 共享解码器/语义 IR**：`tools/gen_opcodes.py` + `src/jit_compile.c` 的语义是唯一
  真相源，AOT 只换后端。否则两套语义必然发散——本仓库 DiffTest 已经用三个真 bug 证明了这一点。
- **保留解释器回退**：不做"100% 覆盖"的承诺，ROM 里永远有静态分析看不见的路径。

**验收门（与 `labs/jit6502` 同构）与 kill criteria：**

| 门 | 判据 |
|---|---|
| 正确性 1 | Klaus 三条路径（interp/JIT/AOT）指令数完全一致：30,646,176 |
| 正确性 2 | 定点 N 条指令后 `pc/a/x/y/sp/p/cycles` 与内存 64 KiB 完全一致 |
| 正确性 3 | 6 个 ROM 的 200 帧截图 SHA-256 与 NEMU 参考一致 |
| 正确性 4 | 与 GEM6502 的逐指令 trace 对拍通过（`scripts/difftest_gem6502.sh`） |
| 性能 1（继续投入） | CPU 负载 ≥ 600 MIPS（即 chained JIT 水平） |
| 性能 2（继续投入） | `falling.nes` 600 帧 ≤ 0.35 s 且帧哈希精确 |
| **止损** | 上述任一条达不到，或需要为每个 ROM 手工标注符号才能跑通（N64Recomp 路线），则放弃，只保留文档结论 |

保守预期：**CPU 负载 600–850 MIPS（相对解释器 8–11×），NES 端到端 0.32–0.38 s
（相对当前 JIT 1.15–1.35×）**。如果实测落在这个区间下沿，这件事就不该进主干。

---

## 8. 更便宜的替代路线（先做这些，再决定是否 AOT）

按"每单位工程量换到的吞吐"排序：

1. **并行**：ROM 矩阵回归用 N 进程跑（`make -j` 或 xargs -P），线性扩展，零风险。
   AOT 最多给 1.3×，并行给 8×。
2. **消除每块一次的 `nes_sync()`**：改成"按周期预算批量推进"，让 PPU 推进与块边界解耦
   （AOT 想做的静态同步点，在 JIT 里也能用"预算计数器"近似做到）。
3. **`$2000–$4017` 特化**：把 `bit $2002` 轮询、`$2005/$2006` 写入做成直接字段访问 +
   内联 sync，跳过通用 bus helper。JIT README 显示这类外部成本曾让 JIT 比解释器慢 17%。
4. **APU 批量采样**：当前 APU 的精度/速度取舍已经有 1 个采样的偏差（README 记录），
   与其上 AOT，不如把采样边界做成批量。
5. **Verilator 侧调优**（§4 表）：DiffTest 真正的瓶颈在这里，收益 2–10×。

---

## 9. 复现命令

```sh
cd labs/jit6502 && make -s

# 纯 CPU：三种引擎（1 亿条指令）
./bin/jit6502 bench bench/arith_loop.bin --engine=interp
./bin/jit6502 bench bench/arith_loop.bin --engine=jit --no-chain --code=8000-10000
./bin/jit6502 bench bench/arith_loop.bin --engine=jit --chain    --code=8000-10000

# NES 全机：600 帧（链接模式仅作"无派发上界"参考，画面会撕裂）
./bin/jit6502 nes ../../roms/falling.nes --frames=600 --engine=interp
./bin/jit6502 nes ../../roms/falling.nes --frames=600 --engine=jit
./bin/jit6502 nes ../../roms/falling.nes --frames=600 --engine=jit --chain

# 正确性门（Klaus 需要 /tmp/klaus.bin）
make test
```

本文所有数字来自 2026-11-15 的一次本机运行（Apple Silicon / macOS 26.3.1 / clang 21.1.8，
`-O2`）。两处需要说明的差异：

- `labs/jit6502/README.md` 的"Measured results"表里 `--no-chain` 只有 41 MIPS，
  那是 **`getenv("JIT_CHAIN_BUDGET")` 还在每个块执行一次**时的旧数据（README 自己记录了这次修复）；
  修复后本机复测为 318 MIPS。其余列（解释器 ~85、chained ~750）与本机 77 / 644 属正常机器差异。
- 因此本文一律采用本次实测值，不引用该表；结论对 ±15% 的机器差异不敏感，
  但**对"是否已消除每块一次的系统调用/查表"极其敏感**——这本身就是 AOT 论点的证据：
  块边界上的固定开销，才是这类仿真器真正的成本中心。

---

## 10. 判定

```
问：能不能用 AOT 加速模拟仿真？
答：
  ├─ 目标是"6502/NES 游戏跑得更快"？
  │    └─ 不用做。解释器已 129× 实机，JIT 已 1395 fps。AOT 上限 1.4×，收益不可见。
  ├─ 目标是"DiffTest / 验证吞吐"？
  │    ├─ 瓶颈在 RTL 侧 → Verilator（已经是 AOT）。加 --threads、2-state、去 DPI。
  │    └─ 瓶颈在 REF 侧 → 先并行跑 ROM 矩阵，再考虑 AOT（REF 现在有 200× 余量，通常不是瓶颈）。
  ├─ 目标是"cycle-exact 同时要快"？
  │    └─ 值得做 §3.4 的静态同步点 AOT，1–2 周 spike，quantified kill criteria。
  └─ 目标是"GEM6502 时序平台吞吐"？
       └─ 值得做 §5 的"事件图 → 静态调度"生成，先有 G1 基准，再谈数字。
```

**最终判断：AOT 是可行的，而且在本仓库的 RTL 层已经兑现了它最大的价值（~100×）。
但对"模拟仿真"这个模糊表述里的 6502 功能核心，AOT 的天花板被"块链接已经消除了派发成本"
这个事实压到 1.2–1.4×，而该核心早已比实机快两个数量级——所以它是一个**用工程复杂度换
一个不可见数字**的选择，只有"编译期精确同步点"这一个场景例外。**

---

## 11. AOT + JIT 混合分层能再快多少？

通用结论：混合是模拟器的主流形态（FX!32 的 profile 引导、Rosetta 2 的"安装期 AOT + 运行期
JIT 兜底"、Java/V8 的分层编译都是这个结构）。但在**本仓库的 6502 场景里**，混合的增量收益
比一般预期小得多，原因是一条实测事实：

```
falling.nes 跑完整 600 帧（5,640,671 个块）只编译了 131 个块，599 次解释器回退（0.01%）。
```

也就是说：**JIT 的编译成本在这里约等于零**。AOT 最常见的卖点——"省掉运行期编译"——在 6502
上根本不成立。真正被反复支付的是**块边界的派发/序言/回退成本**（实测 23 ns/块），而这块成本
运行期 chaining 已经能消除，AOT 只是把同一件事提前到编译期做。

### 11.1 三层分层的现实收益

| 分层结构 | NES 600 帧 | 相对现状 | 说明 |
|---|---:|---:|---|
| 解释器 only | 0.65 s | 0.66× | 基线 |
| JIT only（不链接，现状默认） | 0.43 s | 1.00× | 帧哈希精确，5.6M 次派发 |
| JIT + 运行期 chaining | 0.30 s | 1.43× | **派发成本的实测下界**，但画面撕裂 |
| **AOT(static) + JIT(dynamic) + interp(cold)** | **0.30–0.35 s** | **1.25–1.43×** | 理论上可在保住帧哈希的前提下逼近 chaining |

关键点：**混合的分层上限 = chaining 的实测值，不是"再快一个数量级"**。0.30 s 不是 AOT 带来的，
是"把派发从热路径上拿掉"带来的；chaining 已经证明了这件事值 1.43×，AOT 只是换一种方式拿到它，
并顺便避开 chaining 的精度损失（静态同步点，§3.4）。

### 11.2 想真正再往下走，必须离开 CPU 层

600 帧 = 17.87M 个 6502 周期，每帧 29,780 周期 ÷ ~2.7 周期/指令 ≈ 11k 条指令；
按 chained 的 1.55 ns/指令，**一帧的 CPU 模拟只占 17 µs，而一帧的预算（60 fps）是 16.7 ms，
即 0.1%**。把 6502 核心加速到无穷快，端到端最多再省 0.1%；剩下的 99.9% 在
`ppu_step()`、总线 helper、`nes_sync()` 和前端里（§2.2 的 70% 就是这个）。

所以"AOT + JIT 混合能不能进一步优化 EMU 速度"的正确答案是：

- **对 6502 执行引擎**：能，1.25–1.43×，且这 1.43× 是 chaining 已经测量出的硬上界；
- **对整机吞吐**：不能。除非同时动 PPU/总线同步路径，否则混合分层只是把 0.1% 的成本再压缩；
- **对"精确 + 快"这个组合**：能，而且这是混合唯一不可替代的价值——AOT 负责 ROM 代码的
  静态同步点与 I/O 特化，JIT 负责 bank 切换/RAM 代码/运行期发现的间接目标，解释器兜底冷路径。

### 11.3 混合的代价

| 成本项 | 量级 |
|---|---|
| 第二套代码生成后端（或参数化的同一后端） | 与 `jit_compile.c` 同量级，~1–2k 行 |
| 语义同步风险 | 高：本仓库 DiffTest 已用 3 个真 bug 证明双后端会发散，必须共享解码器/语义 IR |
| 分层守卫开销（版本检查、间接跳转查表） | 5–15%；做不好会把 chaining 省的又吐回去 |
| 验证矩阵 | 4 道门 × {interp, JIT, JIT+chain, AOT, hybrid} × 6+ ROM |

### 11.4 判定

| 问 | 答 |
|---|---|
| AOT + JIT 混合能否"进一步"优化 EMU？ | **能，但对 6502 只有 1.25–1.43×，且上界已被 chaining 实测锁死** |
| 值得为速度做吗？ | 不值得：0.1% 的 CPU 占比意味着这 1.4× 对整机几乎不可见 |
| 值得为什么做？ | 值得为**"精确 + 快"**做：AOT 静态同步点 + JIT 动态兜底，是唯一同时满足帧哈希精确与 chaining 速度的结构 |
| 若目标是整机吞吐 | 离开 CPU 层：PPU/总线同步、APU 批量、Verilator 侧 `--threads`/2-state，收益 2–10× |

---

## 12. JIT 版 EMU 能否作为 RTL 的 golden line？

**不能。它可以当"第二意见"和"加速器"，不能当"记录权威"（golden of record）。**

### 12.1 三个决定性理由

**（1）逻辑上循环。** 本仓库的 JIT 本身是被 REF 验出来的：Klaus 指令数一致、定点状态一致、
20k 指令随机差分（含 64 KiB 全内存）、6 ROM 帧哈希一致、GEM6502 逐指令 trace 对拍。
用"被验证对象"去验证 RTL，等于把 RTL 的正确性锚在一个没有独立来源的实现上。
`docs/nes-golden-reference/test-plan.md` 写得很明确：**"REF and DUT must not be the same binary"**；
`docs/open6502-chip/difftest.md` §6 还要求 REF 必须被 Klaus / 外部测试 ROM / 硬件捕获标定。

**（2）精度上不够。** DiffTest 要的四个同步点（instruction / scanline / frame / audio），
JIT 现在至少有两个给不出（`labs/jit6502/README.md`"Known approximations"）：

| DiffTest 要求 | JIT 现状 | 是否可用 |
|---|---|---|
| 指令级 retire（PC/A/X/Y/SP/P/cycles） | 可以，但必须 `--no-chain` 且逐指令切块 | 可用但退化 |
| bus 级事件（addr/data/we/tick） | 块内访问不可见，indexed/indirect **不切块**，PPU 可滞后一个块 | **不可用** |
| scanline 级（v/t/x/w/OAM/status） | 未实现逐 scanline 导出 | **不可用** |
| audio 级 sample-exact | 300 帧 220,135 vs NEMU 220,134，波形相关 **0.67** | **不可用** |
| mapper（bank/IRQ/PRG RAM） | 仅 CPU 侧 0/1/2/3/4/7 | 不完整 |
| save/replay、mismatch 复现 | 无 | **不可用** |

**（3）结构上自相矛盾。** golden 需要"每条指令、每次总线访问都可观测"；JIT 的收益恰恰来自
"把 N 条指令并成一个原子块、块内不产生可观测事件"。要让 JIT 满足第（2）条，就得给每次访问加
hook、按 I/O 边界重新切块——**收益（1.43×）立刻消失**，只剩维护两套语义的成本。

而且不需要这笔速度：REF 侧现在有 **200× 余量**（§2.3）。**golden 应该选最笨、最可读、最可审计的
实现，不是最快的那个。**

### 12.2 唯一正当的"编译模型进 golden 链路"的结构

不是"用 JIT 当 golden"，而是"**golden = 语义 IR/解释器，AOT/JIT 只是它的 lowering**"，
即业界说的 **compiled ISS**。准入门槛是四条，缺一不可：

1. 每个 opcode 的 lowering 有逐条差分证明（本仓库已有：152 条官方指令 × 20k 随机差分，
   状态 + 周期 + 64 KiB 内存全比）；
2. **不跨指令重排可观测事件**：bus 访问、周期计数、PPU 同步点必须按程序序发射；
3. 每次 DiffTest 跑前重新验证 lowering（版本锁 + 门禁），而不是"上次跑过"；
4. REF 的 commit / config / 状态版本 / ROM 哈希全部记录（difftest.md §6 已要求）。

满足这四条时，跑得快的是"同一个 golden 的编译产物"，语义来源仍是解释器——这才是可以接受的。

### 12.3 推荐用法：三方差分，而不是替换 golden

```
        RTL (Verilator DUT)
              │  instruction/bus/scanline/frame/audio 同步点
              ▼
        REF = 解释器（golden of record）
              │  同一条 trace
              ▼
        JIT（第二实现，仅作交叉检查）
```

- **三方不一致** → 先查 REF，再查 JIT；两者一致而 RTL 不一致，才判 RTL 错。
- 这比"JIT 单挑 RTL"强，因为 JIT 与解释器是同一 spec 的两条独立实现路径，
  分歧本身就是最有价值的信号（本仓库 DiffTest 已经用这种方式抓到 3 个真 bug）。
- JIT 的另一个正当用途是**长跑预筛**：用 1.4× 的吞吐跑几亿周期找崩溃/挂死/帧漂移，
  发现可疑点再回解释器逐指令确认。

### 12.4 判定

| 问题 | 答案 |
|---|---|
| JIT 能当 RTL 的 golden line 吗？ | **不能**：循环论证 + 缺 bus/scanline/audio 精度 + 与 JIT 结构冲突 |
| 能当什么？ | 第二实现（N-version 交叉检查）、长跑预筛加速器、帧级快速回归的 DUT |
| 需要 JIT 的速度进 golden 链路吗？ | 不需要：REF 有 200× 余量，速度不是约束 |
| 如果一定要编译产物进链路？ | 走 compiled ISS：golden = 解释器语义，JIT/AOT 只是 lowering，且每次门禁重验 |
| golden 的最终权威是谁？ | 不是任何软件模型：Klaus + 外部测试 ROM + **硬件捕获**（test-plan L6） |

---

## 13. NEMU 与 GEM6502 能当 golden-reference 吗？

**要分观察量回答：NEMU 在"CPU 指令语义"这一格可以（有第三方标定），其余绝大多数格不够格；
GEM6502 按它自己的文档，目前一格都不够格。** 而且有两个观察量（dot 级 PPU 时序、APU 逐采样）
在仓库里**根本还没有权威**——这不是"选谁"的问题。

### 13.1 权威矩阵（按 `docs/nes-golden-reference/feature-matrix.md` 的实测状态）

| 观察量 | NEMU | GEM6502 | 现状判定 |
|---|---|---|---|
| 官方 6502 指令/寻址 | **A1 DONE**（Klaus `PC=3469`，第三方测试） | legacy 解释器，同源 | **NEMU 可为权威（此格）** |
| dummy read / RMW 双写 / 总线周期 | A3/A4 **PARTIAL**（有周期、无 bus trace、无专项测试） | 无 | **无权威** |
| NMI/IRQ 采样、BRK/RTI 边界 | A5/A6 PARTIAL | 无 | 无权威 |
| OAM DMA 513 周期/对齐、DMC DMA stall | A7/A8 **PARTIAL**（"513-cycle approximation"，DMC stall 缺失） | 无 | 无权威 |
| PPU dot 级背景 fetch/shift pipeline | C1/C5 **PARTIAL**（"no fetch/shift registers"，精灵按 scanline 合成） | 复用 NEMU | **无权威** |
| sprite 0 hit / overflow 精确 dot | C7/C8 PARTIAL | 复用 NEMU | 无权威 |
| APU 逐采样 / 混音 / frame sequencer | D1–D7 **全部 PARTIAL**（混音"approximate"） | 复用 NEMU | **无权威** |
| mapper IRQ（MMC3 A12） | E7/E12 PARTIAL（"no A12 exact timing"） | 仅 CPU 侧 0/1/2/3/4/7 | 无权威 |
| 外部测试 ROM 套件 | F12 **MISSING**（只有 Klaus/开源游戏/自测） | 无 | **无权威** |
| 硬件捕获对齐 | F10 **MISSING** | 无 | **无权威** |
| 帧级像素（已验证的 6 个 ROM） | F4/F5 DONE（60 帧 parity） | 逐字节一致 | **NEMU 可为权威（范围受限）** |
| 芯片级时序（contention/仲裁/时钟域） | 不存在 | G0 partial、G1 experimental、**无 timing CPU、无校准 chip profile** | **无权威** |

### 13.2 三个不能忽视的结构问题

**（1）独立性已被自己破坏。** `docs/open6502-chip/difftest.md` §6 原文：
"The current NEMU REF and frontend share PPU/APU code; it is provisional."
同一份 PPU/APU 代码既当 REF 又当前端，等于自己给自己出题。
`labs/jit6502` 的 152 条 opcode 表也是从 NEMU 解码表生成的（`tools/gen_opcodes.py`），
所以 NEMU ↔ JIT 的分歧只能抓到"翻译实现"的 bug，抓不到"解码表"本身的 bug。

**（2）NEMU ↔ RTL 的一致性是"收敛"不是"正确"。** README 记录 RTL 与 NEMU 的 title frame
SHA-256 相同。两个实现调到一致只证明二者同错或同对；在 PPU 没有 fetch/shift 寄存器、
APU 混音是近似的条件下，**同错的可能性恰恰最高**。

**（3）GEM6502 的定位被它自己的文档否掉了。** `GEM6502/README.md`：
"the current implementation is a legacy instruction-driven functional/cycle model, not the
completed chip timing platform… Games, cycle counts, and NEMU trace matches are useful
regression assets, but do not establish chip-level timing fidelity."
`spec/development-status.md`：G0 partial 未冻结、G1 事件队列 "O(n) sorted array… not performance
calibrated"、无 timing CPU / 无 DMA 引擎 / 无校准 profile、"No gate is promoted to complete"。
**它是一个正在施工的 DUT 和回归资产，不是权威。**

### 13.3 要让它们够格，缺的是同一件东西：硬件捕获

矩阵里所有"无权威"的格子都可以被同一个动作补上——**逻辑分析仪/FPGA 侧的硬件捕获（F10/L6）**：

- 抓真实 2A03/2C02 的 bus 周期、PPU dot 事件、APU 采样、mapper IRQ 时刻；
- 把捕获作为**独立锚点**，再回头标定 NEMU 的 PPU/APU（而不是让 NEMU 和 RTL 互相对齐）；
- 在此之前，NEMU 只能声明为"CPU 语义权威 + 受限功能权威（provisional）"，
  任何"cycle-exact"的说法都必须打上"未标定"标记。

配套的最小动作（按性价比）：

| 动作 | 补上的格子 | 成本 |
|---|---|---|
| 独立 REF 构建（`CONFIG_NES_REF`，不含前端 SDL/UI 代码） | 独立性 | 低 |
| 接入 blargg `ppu_vbl_nmi`/`sprite_hit`/`apu_*`/`dmc` 等外部 ROM（F12） | dot/APU/DMA | 低 |
| 加 dot/bus trace（F11）与 PPU/APU/mapper DiffTest（F8/F9） | 可观测性 | 中 |
| 硬件捕获对齐（F10，对应 test-plan L6） | **所有"无权威"格** | 高，但唯一不可替代 |

### 13.4 判定

| 问 | 答 |
|---|---|
| NEMU 能当 golden 吗？ | **能当"CPU 指令语义"和"受限帧级功能"的权威**；PPU dot / APU / DMA / mapper IRQ 不够格（文档自认 PARTIAL） |
| GEM6502 能当 golden 吗？ | **不能**：legacy 功能/周期模型，G0 未冻结、G1 实验性、无 timing CPU、无校准 profile；且被 NEMU 标定过，属相关性证据 |
| 两者能互相标定吗？ | 不能作为权威来源，只能做 N 版本交叉检查（分歧有价值，一致不等于正确） |
| 缺的那一块是什么？ | **硬件捕获**：它是把"软件模型互相对齐"变成"对物理实现标定"的唯一桥梁 |
| 现阶段怎么写文档才诚实？ | 按 §13.1 的矩阵逐格标注权威与状态，所有 PARTIAL/MISSING 格禁止出现 "cycle-exact" 字样 |

---

## 14. "完成率很低"该怎么读

对 `docs/nes-golden-reference/feature-matrix.md` 逐项统计（73 项）：

| 范围 | 项数 | DONE | PARTIAL | MISSING | 加权分\* |
|---|---:|---:|---:|---:|---:|
| A CPU | 10 | 2 | 8 | 0 | 60% |
| B 总线/输入 | 10 | 4 | 3 | 3 | 55% |
| C PPU | 17 | 6 | 10 | 1 | 65% |
| D APU | 11 | 2 | 9 | 0 | 59% |
| E 卡带/mapper | 13 | 8 | 2 | 3 | 69% |
| F 平台/golden | 12 | 5 | 1 | 6 | 46% |
| **合计** | **73** | **27（37%）** | **33（45%）** | **13（18%）** | **60%** |

\* 加权分 = (DONE + 0.5×PARTIAL) / 总数。

按用途再切一刀，结论会完全不同：

| 口径 | 结果 | 含义 |
|---|---:|---|
| 全表条目 | 37% DONE | 把 Zapper、Four Score、T2 mapper 和 CPU 同权——**误导性口径** |
| 芯片 L1/L2（59 项，排掉可选外设与验证基建） | **0 MISSING**，加权 **73%** | 功能覆盖面其实不低 |
| 验证基建 L4（C17、F8–F12，6 项） | **0%** | 外部 ROM 套件、PPU/APU DiffTest、dot/bus trace、硬件捕获**一项都没有** |
| 可选扩展 L3（8 项） | 6% | PAL、NES2.0、电池存档、T2 mapper、扩展音频 |
| **可作为"权威"的格子**（§13.1） | **≈10%** | 只有 Klaus 覆盖的 CPU 语义 + 6 ROM 的帧级 parity 有第三方/独立证据 |

三种口径的差别不是文字游戏：

- **按条目** 37% → 看起来"很低"；
- **按加权** 60%（芯片 L1/L2 73%）→ 看起来"还不错"；
- **按"能当 golden 用"** ≈10% → 才是 DiffTest 真正能依赖的部分。

原因在于 **PARTIAL 不是"完成一半"**。PARTIAL 在 feature matrix 里的意思是"功能在、时序未证"：

- A3/A4：有周期计数，但无 bus trace、无 dummy read 专项测试；
- A7/A8：DMA 是"513-cycle approximation"，DMC stall 缺失；
- C1/C5：无 fetch/shift 寄存器，精灵按 scanline 合成；
- D1–D7：混音"approximate"；
- E7/E12：MMC3 无 A12 精确时序。

**对"跑游戏"来说 PARTIAL 够用；对"当权威"来说 PARTIAL 等于 0**——这正是 §13 说它们不能当
golden 的量化版本。

### 14.1 与 README 的 90% 不一致

顶层 `README.md` 的进度表写 **"NEMU reference 90%"**，但没有任何指向 feature matrix 的推导。
按矩阵，同一对象的数字是 37% DONE / 60% 加权 / ≈10% 可当权威。**两套口径必须对齐**，
否则"90%"只是一个无法复核的观感。建议把进度表改成矩阵导出值，并在表头写明口径
（条目比 / 加权 / 权威覆盖）。

### 14.2 GEM6502 不能用百分比回答

`GEM6502/docs/requirements.md` §3 明确写了："以上为代码审查基线，**不用行数、游戏数量或
虚构完成百分比衡量芯片仿真成熟度**"。所以正确的读法是按 G0–G6 门禁：

| 门禁 | 状态 |
|---|---|
| G0 规格冻结 | partial，未冻结（无独立来源的 profile 契约） |
| G1 事件内核 | experimental（O(n) 排序数组、未做性能校准） |
| G2 时序互连 | partial（单目标 RAM 总线，FM/MMIO、abort、公平性未完成） |
| G3 时序 CPU | **未开始** |
| G4 SoC 交互 | 未开始 |
| G5 实验工程 | 未开始 |
| G6 校准发布 | 未开始 |

**0/7 通过，2 项在施工，G3 起全部未开始。** 另外它有两个身份需要分开算：

| 身份 | 状态 |
|---|---|
| v0.1.0 legacy 功能模拟器（可玩、mapper 0/1/2/3/4/7、SDL 前端） | 功能上大致与 NEMU 同级，但从未按矩阵逐项审计 |
| v0.2.0 时序仿真平台（目标产品） | 门禁 0/7，`README.md` 自认 "does not establish chip-level timing fidelity" |

### 14.3 一句话

**NEMU 的功能完成度不低（L1/L2 零缺失、加权 73%），低的是"时序精度"和"验证基建"；
GEM6502 作为功能模拟器起步，作为时序平台则是 0/7 门禁。把"完成率"用于决策时，
唯一有意义的口径是"有多少格子能当权威"——目前 ≈10%。**
