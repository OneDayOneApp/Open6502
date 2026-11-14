# 6502 → AArch64 JIT 实验

这是一个**教学/验证性实验**：从零写一个 6502 解释器，再为它加一个把 6502 基础块
翻译成 AArch64 机器码的 JIT，用可复现的数据回答三个问题：

1. JIT 到底把“取指 → 译码 → 分派 → 执行”变成了什么？
2. 在 6502 这种极简指令集上，JIT 实际能快多少、在什么条件下反而更慢？
3. JIT 的代价是什么——代码缓存、自修改代码、调试？

实验大部分独立：CPU/JIT/基准不依赖 NEMU；NES 集成阶段把项目自己的 PPU 与
mapper 实现（`NEMU/src/nes/`）搬进来复用，以便逐帧对照。不参与主仓库构建。

## 快速开始

```sh
cd labs/jit6502
make                      # 生成 bin/jit6502
python3 tools/make_bench.py bench

# 下载 Klaus 功能测试（约 64 KiB）到 /tmp/klaus.bin
curl -fsSL -o /tmp/klaus.bin \
  https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/bin_files/6502_functional_test.bin

./bin/jit6502 klaus /tmp/klaus.bin --engine=interp           # 解释器基线
./bin/jit6502 klaus /tmp/klaus.bin --engine=jit --chain     # 机器码 JIT
./bin/jit6502 selfcheck 20000                                # 随机差分测试
make test                                                    # 以上全部 + 基准
```

常用参数：

| 参数 | 含义 |
|---|---|
| `--engine=interp\|jit` | 选择执行引擎 |
| `--chain` / `--no-chain` | 是否启用块链接（吞吐 vs 精确指令计数） |
| `--code=LO-HI` | 声明“可能被写入的代码区间”，默认 `0x0400-0x8000` |
| `--max-inst=N` | 指令预算（`--no-chain` 时严格生效） |
| `--stop-pc=ADDR` | 到达某 PC 时停止（块不会跨过该点） |
| `--trace=FILE` | 逐指令/逐块状态（JIT 只记录块入口） |
| `--mem=ADDR:LEN` | 运行后 dump 内存，便于核对 store |
| `--stats` | 打印编译块数与失效次数 |

## 代码结构

| 文件 | 作用 |
|---|---|
| `src/cpu.h` / `src/cpu.c` | 64 KiB 平铺内存的 6502 状态机 |
| `src/interp.c` | 参考解释器（含十进制 ADC/SBC、页跨越周期、JMP 间接页 bug） |
| `src/opcodes_gen.h` | 由 `tools/gen_opcodes.py` 从 NEMU 的 6502 解码表生成的 152 条官方指令表 |
| `src/arm64.h` | AArch64 编码器 + 带标签回填的 Emitter |
| `src/jit_compile.c` | 基础块翻译器：把 6502 指令映射到机器码 |
| `src/jit_runtime.c` | 可执行内存、块缓存、页级失效、派发与块链接 |
| `src/jit_helpers.c` | 十进制 ADC/SBC 的 C 辅助函数 |
| `src/selftest.c` | 单指令随机差分测试 |
| `src/main.c` | 命令行 |
| `tools/enccheck.c` | 用 `llvm-mc` 的编码核对编码器（开发期使用） |
| `tools/make_bench.py` | 生成基准程序镜像 |
| `nes/nes_system.c` | NES 总线、手柄、PPU 同步、中断挂起 |
| `nes/ppu.c` / `nes/cart.c` | 复用 NEMU 的 PPU 与 mapper（逐帧一致） |
| `nes/frontend_sdl.c` | 可选的 SDL 窗口前端 |

## JIT 概念 → 代码对应

| 概念 | 在这个实验里 |
|---|---|
| 解释器分派开销 | `interp.c` 的 `switch (info.op)`：每条指令一次取指、一次表查、一次分支 |
| 基础块（basic block） | `jit_compile()` 从 `start_pc` 线序翻译，遇到分支/跳转/调用/未支持指令即结束 |
| 客户机状态放在宿主寄存器 | `x19=Cpu*`、`x20=mem`、`w21..w25=A/X/Y/SP/P`、`x26=mem+PC`、`x27=cycles` |
| PC 作为宿主指针 | 让立即数/零页操作数可以一条 `ldrb wN, [x26, #disp]` 取到 |
| 代码缓存 | `g_map[65536]`：PC → 编译好的函数 |
| 直接映射 vs 哈希 | 64K 数组，命中就是一次 O(1) 查表 |
| 块链接（chaining） | 跳转目标已编译时直接 `br` 到目标块的 resume 点，跳过“收尾 → 派发 → 重建” |
| 自修改代码失效 | 翻译后的 store 写入代码区间时，记录页号并退出块；派发器只丢弃该页的块 |
| 回退路径 | 未翻译指令（BRK/RTI/JMP 间接）交给解释器执行一步 |
| 指令计数 | 每个块返回实际执行条数（分支提前退出时回填），保证预算与停止点精确 |

寄存器约定（块内被调用者保存，prologue/epilogue 负责进出 C 世界）：

```text
x19 Cpu*     x20 mem        w21 A       w22 X       w23 Y
w24 SP       w25 P          x26 mem+PC  x27 cycles  w28 0x7D（清 N/Z）
x29 resume 表基址            w0  已执行指令数（跨块累加）
```

## 正确性

- **Klaus 功能测试**：三条路径（解释器、无链接 JIT、链接 JIT）都停在 `PC=$3469`，
  且**执行的指令总数完全一致**：`30,646,176`。
- **固定预算状态对比**：同一镜像跑 N 条指令，解释器与 JIT 的
  `pc/a/x/y/sp/p/cycles` 逐字段相同（`make test` 覆盖 N=100k/1M/5M）。
- **随机差分测试**：每个 opcode 配随机寄存器状态与随机内存，解释器与 JIT 各执行
  一条指令，比较状态、周期数**以及整个 64K 内存**；`selfcheck 20000` 通过。

## 实测结果

机器：Apple M2，macOS 26.3，`clang -O2`。MIPS = 每秒执行的 6502 指令数。

| 负载 | 解释器 | JIT（无链接） | JIT（块链接） | 加速比 |
|---|---:|---:|---:|---:|
| Klaus 功能测试 | ≈85 | ≈145 | **≈750** | **≈8.8×** |
| `arith_loop`（ADC/STA/INX/BNE） | ≈100 | ≈41 | **≈620** | **≈6.2×** |
| `copy_loop`（间接寻址拷贝） | ≈110 | ≈39 | **≈827** | **≈7.5×** |
| `call_loop`（JSR/RTS） | ≈104 | ≈27 | **≈681** | **≈6.5×** |

数值随机器负载有 ±15% 波动，重点是量级差异；每次运行 `make test` 都会重新打印。
Klaus 的缓存行为：`1697` 个块编译、`254` 次页级失效、`0` 次全量清空。

### 三个能直接复现的结论

**1. 没有块链接，JIT 会输给一个写得不错的解释器。**
`--no-chain` 时每个块都要付“保存/恢复寄存器 + 返回派发器”的固定成本；
基准循环的块只有 4~6 条指令，摊销不开。

```sh
./bin/jit6502 bench bench/arith_loop.bin --engine=jit --no-chain --code=8000-10000   # ~41 MIPS
./bin/jit6502 bench bench/arith_loop.bin --engine=jit --chain    --code=8000-10000   # ~636 MIPS
```

**2. 失效范围比“有没有 JIT”更影响性能。**
`--code` 声明的区间越大，越多的数据写入被误判成“可能改写了代码”。
把 Klaus 的数据段也标成代码区间（它执行放在 `$0200` 的 `ex_*` 桩），
失效次数从 254 涨到 303,611，速度掉回解释器水平：

```sh
./bin/jit6502 klaus /tmp/klaus.bin --engine=jit --chain --stats                    # ~747 MIPS, 254 flushes
./bin/jit6502 klaus /tmp/klaus.bin --engine=jit --chain --code=200-3800 --stats    #  ~87 MIPS, 303k flushes
```

**3. 全量清空并不必然是灾难——取决于真的发生了多少次代码写入。**
同一个 Klaus 用 `JIT_FULL_FLUSH=1` 强制整表清空仍有 ~747 MIPS，因为只有 254 次
写入落在代码页上；真正的问题是“把数据页当成代码页”。

## NES 集成：让 JIT 跑真实游戏

`nes/` 目录把 NES 机器搭在 lab CPU 周围，分三步：

1. **NES 核心**（`nes/ppu.c`、`nes/apu.c`、`nes/cart.c` 来自本项目 NEMU，
   `nes/nes_system.c` 是总线/手柄/PPU/APU 同步）：
   解释器先跑通。验证方式是与 NEMU 逐帧对照——同样 60 帧、同样 ROM，
   两边截图 **SHA-256 完全相同**，周期数也相同。
2. **JIT 总线模式**（`jit_set_bus_mode`）：
   - `$0000-$1FFF` 走快速路径（直接访问 2 KiB WRAM，地址 `& 0x7FF`）；
   - `$2000+` 调用总线 helper（PPU 寄存器、mapper、手柄）；
   - helper 内部先把 PPU 推进到当前 CPU 周期（`nes_sync`），再读写寄存器，
     所以 `BIT $2002` 轮询 vblank 的时序与解释器一致；
   - 块链接保留，但链接前检查 `JitRuntime::pending`：帧边界或中断挂起时
     立刻回到派发器，NMI 在指令边界被采样；
   - 自修改代码检测在 NES 模式关闭（代码在 ROM，不会变）。
3. **SDL 窗口 + 音频**（`nes/frontend_sdl.c`）：`--window` 直接开窗游玩，
   键盘映射与 NEMU 一致（WASD/方向键、Space/Z、X、C、V、Esc）。
   APU 走 `$4000-$4017`（含 frame IRQ），SDL 以 44.1 kHz 单声道队列播放；
   `--wav=FILE` 可以把同样的音频导出成 WAV 做验证。

```sh
# 无窗口对照（同一条命令可切换引擎）
./bin/jit6502 nes ../../roms/falling.nes --frames=60 --screenshot=/tmp/i.ppm --engine=interp
./bin/jit6502 nes ../../roms/falling.nes --frames=60 --screenshot=/tmp/j.ppm --engine=jit
cmp /tmp/i.ppm /tmp/j.ppm        # 应完全一致

# 用 JIT 开窗游玩（带声音）
./bin/jit6502 nes ../../roms/falling.nes --engine=jit --window --scale=3

# 导出音频做检查
./bin/jit6502 nes ../../roms/falling.nes --frames=300 --wav=/tmp/game.wav --engine=jit
```

实测（`falling.nes`，600 帧 = 10 秒游戏时间，Apple M2，`clang -O2`，默认 `--no-chain`）：

| 引擎 | 600 帧耗时 | 吞吐 | 备注 |
|---|---:|---:|---|
| 解释器 | ≈0.64 s | ≈940 fps | |
| JIT | ≈0.44 s | ≈1380 fps | 131 个块、599 次解释器回退、0 次失效 |

NES 上的加速比（≈1.5×）远小于纯 CPU 负载（6–9×），原因很直接：每帧有大量
`$2002` 轮询和 PPU 寄存器访问，都要走 helper、都要同步 PPU；块被 I/O 和分支切得很碎
（`$C0CA` 的循环每块只有 3 条指令左右，600 帧要派发 5.6M 次）。
这正是“JIT 不是万灵药”的现场证据——但把下面这一节的外部开销压下去之后，
JIT 在 NES 上才终于从“比解释器还慢”翻到“快 1.5×”。

## NES 性能优化：先量再改

最初 `falling.nes` 的 600 帧耗时是 **JIT ≈1.37 s、解释器 ≈1.14 s**：JIT 反而是输的。
用 `sample` 抓 profile 后原因很清楚——**78% 的时间不在 JIT 里，而在 `nes_sync()`
每块调用一次的 `ppu_step()` 里**：

```text
jit_run 92%
  nes_sync 84%
    ppu_step 78%      <- 每帧 61,440 个可见点，每点重新取 tile/attribute/pattern
```

PPU 原本是“每个点重新取一次图块”：一帧 61k 个点，每个点 4 次间接函数调用
（`nt_index` ×2、`chr_read` ×2），相当于把硬件每 8 个点取一次的数据取了 8 遍。
于是按“先去掉外部开销，再改热点”的顺序做了四件事，每步都用 6 个 ROM 的
**逐字节相同的帧**做验收：

| 改动 | 说明 |
|---|---|
| 缓存 `getenv` | 派发器每个块都读 `JIT_CHAIN_BUDGET`，600 帧要读 5.6M 次；改成初始化时读一次 |
| PPU 图块缓存 | 同一行的同一个 tile 连续 8 个点复用；`v` 变化（点 256/257、换行）时失效 |
| CHR 直取 | mapper 0 不做 bank 切换，pattern 读取直接索引 `cart->chr[]`，省掉每次的间接调用和取模 |
| 扫描线批量绘制 | 可见行的点不再逐点判事件，改成按段批量画；事件点（0/1/256/257/339）逐个显式处理 |

顺带删掉了 `prepare_scanline()` 的整行预填充：点 1..256 本来就会覆盖全部 256 个像素，
预填充只是把同样的工作做两遍。

结果（同一台机器、同一 ROM、同一命令，交替多次取最小值）：

| | 优化前 | 优化后 | 倍数 |
|---|---:|---:|---:|
| 解释器 | 1.14 s | 0.64 s | 1.8× |
| JIT | 1.37 s | 0.44 s | 3.2× |
| JIT / 解释器 | 0.83×（更慢） | 1.47× | — |

注意这两条曲线不是等比例改善的：PPU 的开销对两个引擎一样重，所以按“JIT 相对
解释器”算，收益来自 JIT 侧省掉的 `getenv` 和更少的分派；按绝对帧率算，大头是 PPU。

正确性验收（`make test` 全绿）：

```text
Klaus            interp / jit-no-chain / jit-chain 全部 PASS，指令数一致
状态对比         100k / 1M / 5M 指令处逐字段相同
随机差分         20000 条指令，状态 + 周期 + 全 64K 内存一致
NES              falling.nes 第 60 帧与 NEMU 参考 SHA-256 相同，音频样本数相同
GEM6502 DiffTest 逐指令 trace + 检查点全部一致（PPU scanline/dot 也在比对字段里）
```

帧级回归用 6 个 ROM 交叉验证（`falling`、`hanoi`、`file-fixers`、`galaxypatrol`、
`stallar`、`table_tennis`）：解释器与 JIT 的 200 帧截图 SHA-256 全部相同——
PPU 的改动只要有一处时序偏差，这里立刻会红。

## DiffTest：以 GEM6502 为参考模型

`scripts/difftest_gem6502.sh` 把 JIT6502 与仓库的 GEM6502 快速模型做锁步比对：

| 层 | 做法 | 覆盖 |
|---|---|---|
| 逐指令 trace | 双方 `--trace-state` 输出同样 16 个字段（pc/a/x/y/sp/p、6 个零页单元、cycles、PPU scanline/dot、NMI） | JIT 单指令块语义、PPU/APU 同步、周期 |
| 指令检查点 | 双方按**步数**对齐到 N 条指令，比较最终状态 | 多指令块（JIT 开启 `--no-chain` 以保证预算精确） |
| 帧级运行 | 同帧数运行，比较 PC | 链接模式下的稳定性 |

```sh
make difftest                  # 默认 falling.nes, 200000 步
./scripts/difftest_gem6502.sh ../../roms/falling.nes 500000 120
```

这套比对在开发中抓到三个真问题：

1. **OAM DMA 的 513 周期被 JIT 丢掉**：bus helper 直接改 `Cpu.cycles`，而块 epilogue 随后用寄存器里的 `x27` 覆盖它。改为记 `extra_cycles`、由同步钩子吸收。
2. **NES 模式里 `--no-chain` 被覆盖**：`cmd_nes` 无条件开启链接，导致预算被链式执行越过、检查点对不齐。
3. **对齐口径**：GEM6502 的 `instructions` 计数不含 DMA 停顿步，`--max-inst` 会多走 18 步；两边改用 `--max-steps` / 步数对齐后才逐位一致。

### NES 模式的已知近似

- 仅实现 mapper 0/1/2/3/4/7 的 CPU 侧读写；APU 已接入，但采样边界与 NEMU
  不是逐样本一致：同样 300 帧，本实验 220,135 个采样、NEMU 220,134 个，
  波形相关度约 0.67（APU 的步进粒度是“块 + I/O 访问”，不是逐指令）。
- 索引/间接寻址的访存不再切块，PPU 最多晚一个块同步；常见游戏不受影响，
  但依赖逐周期精确的测试 ROM 可能对不上。
- 没有存档、回放、录音；窗口为固定 3 倍缩放。
- NES 模式下**默认不链接**：链接只在 I/O 访问时推进 PPU，长时间纯 RAM 代码会
  让 vblank NMI 迟到，帧内寄存器写入错位，表现为画面上的横向黑色条纹。
  实测无链 600 帧 ≈1.37 s（约 438 fps），足够 60 fps 游玩。
  `--chain` 仍是显式实验选项（吞吐更高，但画面可能撕裂）。

## 已知局限

- 只覆盖官方 NMOS 6502 指令；中断（NMI/IRQ）在 NES 模式下于指令边界采样，
  纯 CPU 模式没有中断源。
- 平铺 64 KiB 模式仍然是纯 CPU 实验（跑 Klaus/基准）；NES 模式才有 PPU/APU/总线。
- 块不跨 256 字节页（为了页级失效成立），块上限 48 条指令。
- 没有寄存器分配器和常量池：寄存器是固定映射，辅助函数地址用 `movz/movk` 现拼。
- `--code` 必须覆盖“会被写入的代码”；只执行、不写入的代码可以在窗口外。
- 链接模式为了吞吐放松了 `--max-inst` 的严格性（链式执行会小幅越过预算），
  需要精确计数时用 `--no-chain`。

## 和主项目的关系

这个实验的结论和仓库主线是分开的：

- **NEMU / RTL 验证链**不需要 JIT。RTL 侧慢几个数量级，参考模型不是瓶颈；而且
  DiffTest 要的是逐指令、逐周期可复现，JIT 只会增加失配面。
- **浏览器播放器**不需要 JIT：wasm 里不能自生成代码，浏览器本身已经在 JIT wasm。
- 真正值得考虑 JIT 的场景是“大规模回归 / 模糊测试 / 低端设备”，
  而且要先做便宜的优化——缓存 `getenv`、减少每指令分派、按扫描线批处理 PPU，
  这三件事在这个实验里都已经落地（见上文“NES 性能优化”），收益比调 JIT 本身还大。

## 下一步可以做什么

1. **I/O 快路径**：给 `$2002`/`$2007` 做内联快路径，减少 helper 与 PPU 同步开销
   （现在 NES 上是 ≈1.5×，这块仍是大头：每帧 5.6M 次派发、9.4k 次 `nes_sync`）。
1. **链接 + 周期看门狗**：`--chain` 能再快一截，但目前会越过 vblank，帧对不上；
   在 resume 点加“距上次同步超过 N 周期就退回派发器”的检查，才能既链接又保时序。
2. **音频逐样本对齐**：让 APU 的步进边界与 NEMU 一致，把波形相关度从 0.67 提上去。
3. 给块加寄存器分配；用页表 + 写保护替代“写入即失效”。
4. 指令级 trace 对比（当前 JIT 只能做预算级对比）。
