# am6502：给 6502/NES 的抽象机

**状态：M1–M3 已实现。** 平坦镜像能跑 `NEMU/build/6502EMU`；卡带镜像在 NEMU、
GEM6502 和 RTL bench 上用同一份镜像、同一套判定启动；`make difftest` 逐条比对
NEMU 与 RTL 并指出首个分歧。本文其余部分仍是目标而不是现状，已完成/未完成的划分
见 README。

最后更新：2026-11-16。

---

## 1. 这个 lab 要解决的问题

Open6502 现在已经有黄金参考模型（`NEMU/build/6502EMU`）、周期模型（`GEM6502`）
和 RTL SoC（`rtl/`）。缺的是**把同一个程序送进这三者的一条路**。今天有三条互不
相干的路径：

| 路径 | 生产者 | 镜像 | 装载方式 |
|---|---|---|---|
| Klaus 功能测试 | 外部二进制 | `klaus.hex`，64 KiB，reset 向量被 testbench 改成 `$0400` | `tb/open6502_klaus_tb.sv`（`$readmemh`） |
| 游戏画面 | `sim/game/extract_ines.py` | `/tmp/falling/falling_prg.hex` + `falling_chr.hex` | `tb/open6502_game_tb.sv`（`$readmemh`） |
| 单元 / DiffTest | shell 脚本里手写的字节 | `sim/difftest/difftest.bin`（及 `.hex`） | `6502EMU -b --load-addr=...` |

三条路径对“代码放哪、入口在哪、什么算 PASS”各有一套说法。一次性测试无所谓，
但它恰好卡住了 [docs/learning-path.zh-CN.md](../../../docs/learning-path.zh-CN.md)
里的两个内环目标：锁步 DiffTest 要求**两边是同一个镜像**，CI 门禁要求
**每个程序一条命令**。

`nexus-am` 为 RISC-V/香山回答过同一个问题，靠的也是三点：

1. 一层抽象（`am.h`），程序链接它而不是链接某台机器；
2. 一个 ARCH 矩阵（`make ARCH=<isa>-<platform>`），同一份 app 编到每台机器；
3. 一套镜像约定（`0x8000_0000` 的 `.bin` 内存镜像，可选 `0x1000_0000` 的
   flash 镜像），RTL 仿真器和 NEMU 都吃这一套。

这个 lab 只搬这三点，不搬代码。

## 2. 前作与许可边界

`nexus-am`（OpenXiangShan）**没有发布任何 LICENSE 文件**——GitHub API 对该仓库
返回的 `license` 为 null。因此这里不允许复制它的源码、注释、构建文件或链接脚本。
本 lab 取的是*接口传统*：

| nexus-am | am6502 | 复制了什么 |
|---|---|---|
| `am.h`、`_putc`、`_halt`、`_io_read`/`_io_write` | 同名、6502 子集 | 只取名字与签名，独立实现 |
| `make ARCH=riscv64-xs`、`AM_HOME` | `make ARCH=mos6502-nes`、`AM6502_HOME` | 只取约定，不取文件 |
| `.bin` 内存镜像 | 平坦 64 KiB 镜像 / iNES 卡带镜像 | 只取约定 |
| 构建期传入 `mainargs` | `_am_mainargs` 字符串 | 只取概念 |
| `am/arch/*.mk` 选 `am/src/<platform>` | 完全相同的形状 | 只取形状 |
| TRM / IOE / CTE / VME / MPE | 只做 TRM + IOE（见 §3） | 只取 API 名字 |

本目录交付的一切都是为本仓库新写的。如果这套命名造成误解，`am6502` 可以改名
（`nes-am`、`cart-am`），API 不变。

## 3. 范围：只做 TRM + IOE，以及为什么其余不做

AM 有五个扩展，其中三个需要的硬件 6502 没有：

| AM 扩展 | 需要的硬件 | 在 6502 上 |
|---|---|---|
| TRM（图灵机：`_putc`、`_halt`、堆） | 无 | **做** |
| IOE（设备寄存器读写） | 内存映射设备 | **做**，按 NES 的形状 |
| CTE（事件、上下文切换、中断当事件） | 特权态、可保存/恢复的 `_Context` | 不做：没有特权态；NMI/IRQ 是向量，不是上下文 |
| VME（页表、地址空间） | MMU | 不做：16 位平坦总线 |
| MPE（多核、屏障、原子） | hart ID、原子指令、CLINT | 不做：单核 |

这是设计不变量，不是排期问题：**需要 6502 上不存在的硬件才能实现的功能，一律
拒绝，不做假实现。** 一个只保存 5 个寄存器就自称“线程切换”的假 `_Context`，
只会教错东西。

推论：`am6502` 的 app 是单线程、平坦内存的裸机程序——而 NES ROM 本来就只是这个。

## 4. 产物与不变量

两种镜像格式，因为仓库里本来就有两类程序。这是 6502 逼出来的、相对 AM 单
`.bin` 的唯一分歧点。

### 4.1 平坦镜像 —— `build/<app>-mos6502-nemu.bin`

| 属性 | 值 |
|---|---|
| 大小 | 恰好 65,536 字节 |
| 布局 | 一整块 64 KiB RAM/ROM 空间；代码由链接脚本决定 |
| 入口 | `$FFFC`/`$FFFD` 的 reset 向量——**永远不由 testbench 补写** |
| 消费者 | `6502EMU -b -a 0 -r image.bin`（raw 模式；host I/O 在 `$F001-$F003`） |
| 典型 app | Klaus、nestest、ALU 测试，以及一切需要整块 64 KiB 的程序 |

这就是 AM 说的“内存镜像”。注意它与今天 Klaus 流程的区别：向量是构建产物，
所以 `tb/open6502_klaus_tb.sv` 以后可以不再覆盖 `$FFFC`。

### 4.2 卡带镜像 —— `build/<app>-mos6502-nes.nes`

| 属性 | 值 |
|---|---|
| 容器 | iNES：16 字节头 + PRG + CHR |
| PRG | 16 KiB 或 32 KiB（NROM）；更大需要 mapper |
| CHR | 8 KiB（CHR-ROM）或 0（CHR-RAM 程序） |
| 入口 | reset 向量位于最后一个 PRG bank 末尾；CPU 侧映射到 `$C000`/`$8000` |
| 消费者 | `6502EMU game.nes`、`gem6502 game ...`、`tb/open6502_game_tb.sv` |
| 派生产物 | `build/<app>-<arch>.prg.hex`、`.chr.hex`（给 `$readmemh`） |

`.hex` 是**派生**文件，不许手写：`tools/ines.py` 是唯一转换器，取代写死的
`/tmp/falling` 路径，并把 `sim/game/extract_ines.py` 一般化。

### 4.3 调试产物

每次构建都产出 `build/<app>-<arch>.{map,lst}`（M1）；`.elf`/`.sym` 随 C 工具链一起
到来。DiffTest 失败报告应该带上
它们，也让“`$C123` 上到底是哪条指令”这种问题变便宜。

### 4.4 不变量

| 编号 | 不变量 | 谁来检查 |
|---|---|---|
| I1 | 每个镜像都结束在真实程序里：`_halt()` 可达，不会落进 `BRK` | `make test`（退出码） |
| I2 | 平坦镜像恰好 65,536 字节，且 reset 向量指向镜像内部 | `tools/mkimage.py --check` |
| I3 | NES 镜像头合法，PRG/CHR 大小为 8 KiB 的整数倍，mapper 字节与 `ARCH` 一致 | `tools/ines.py --check` |
| I4 | 同一输入构建两次，字节完全一致（无时间戳、无 `$RANDOM`、`.sym` 里符号排序） | CI 双构建 + `sha256sum` |
| I5 | 同一个 `.nes` 不加修改跑通 NEMU、GEM6502、RTL | `make test` 矩阵 |

I4 是最不显眼但最要命的一条：如果产出镜像的人都不能复现镜像，golden 哈希就一文
不值。

## 5. 平台

`ARCH` 是 `<isa>-<platform>`，与 AM 完全一致。平台目录拥有启动代码、链接脚本和
设备驱动；app 只看见 `am.h`。

| ARCH | 平台源码 | 机器模型 | 镜像 | Host I/O | 后端 |
|---|---|---|---|---|---|
| `mos6502-nemu` | `am/src/nemu/` | 平坦 64 KiB，无 PPU/APU | 平坦 `.bin` | `$F001` 输出、`$F002` 退出、`$F003` 输入（6502EMU 已有端口） | `6502EMU -a 0 -r` |
| `mos6502-nes` | `am/src/nes/` | 2 KiB WRAM，PPU `$2000-$2007`，APU `$4000-$4017`，手柄 `$4016/17`，PRG `$8000+` | iNES `.nes`（NROM/UxROM/CNROM/AxROM） | `$4018` 输出、`$4019` 退出（新增，仅 host 侧） | `6502EMU`、`gem6502`、RTL `tb` |
| `mos65c02-nemu`（加分） | 复用 `nemu/` | 平坦 64 KiB + 65C02 指令 | 平坦 `.bin` | 同上 | `6502EMU -C -r` |

内存图（`mos6502-nes`）：

```text
$0000-$00EF  app 零页（工具链分配）
$00F0-$00FF  am6502 运行时零页（见 §6.2）
$0100-$01FF  硬件栈
$0200-$07FF  WRAM：.data/.bss、OAM 影子、软栈、堆
$2000-$2007  PPU 寄存器        （_DEV_PPU）
$4000-$4017  APU 寄存器        （_DEV_APU）
$4018        am6502 host 输出  （写）
$4019        am6502 host 退出  （写后自旋）
$8000-$FFFF  卡带 PRG 空间（NROM：32 KiB 或两个 16 KiB bank）
```

`$4018-$401F` 是 Famicom 的测试寄存器，真机上写入被忽略——所以把 `_putc` 放在
这里，对真实卡带安全，对我们控制的每个仿真器可观测。这就是 6502 版的“UART”：
不是 ISA 定义的设备，而是镜像与宿主之间的约定。

内存图（`mos6502-nemu`）：程序拥有整块 64 KiB；`$F001-$F003` 就是
`NEMU/6502EMU.md` 已经写明的 host I/O。**这个平台不需要给 NEMU 加任何新功能。**
这是故意的：M1 可以不改一行 C 就落地。

## 6. ABI

### 6.1 入口状态

reset 向量跳到平台 `crt0`（汇编）里的 `_start`。进入时 CPU 处于硬件定义状态：
`SP` 未定义、`I` 置位、十进制标志未定义。因此 `crt0` 只做四件事：

1. 置 `SP = $FF`、`CLD`、`SEI`（6502 启动唯一能依赖的三件事）；
2. 清零运行时零页（`$F0-$FF`）和平台 RAM 区（`mos6502-nemu` 上是 `$0200-$03FF`）；
3. 调用 `am_init()`（真实符号；平坦机器上是空操作，nes 平台把 PPU/APU 设置放这里）；
4. 调用 `main()`，然后 `_halt(main())`。

v0 的 `main` 是 `int main(void)`。`argc/argv` **故意不实现**：2 KiB WRAM 的 6502
C 运行时装不下 AM 那套栈上构造的 `argv`，现在的 app 也不需要。取而代之：

```c
const char *am_mainargs(void);   /* 永不为 NULL；未设置时返回 "" */
```

`make ARCH=... mainargs='m2'` 把字符串写进 `.rodata` 的 `_am_mainargs`，对应 AM
的 `-DMAINARGS=`，但不带它的 `argv` 解析。M1 只提供函数、不提供构建变量：它永远
返回 `""`。

### 6.2 运行时零页（`$F0-$FF`）

| 地址 | 名字 | 用途 |
|---|---|---|
| `$F0` | `_am_exit_req` | `_halt` 写端口前置 1 |
| `$F1` | `_am_exit_code` | 最近一次 `_halt` 的参数 |
| `$F2/$F3` | `_am_frames` | NMI/RTI tick 维护的 16 位帧计数 |
| `$F4/$F5` | `_am_ptr` | `_io_*` 辅助例程的临时指针 |
| `$F6-$FF` | 保留 | `_start` 时必须为 0，app 不得触碰 |

app 和工具链用 `$00-$EF`。`cc65`/`llvm-mos` 默认的零页分配通过链接配置被限制在
`$00-$EF` 之内。

### 6.3 栈与堆

- 硬件栈 `$0100-$01FF`：`JSR`/中断压栈用，256 字节，无需配置。
- 软栈（C 局部变量）：由链接配置放在 WRAM 里、堆之下；大小写进 `.map`。
- `_heap`：在 `am.h` 里声明，但还没有任何平台提供（M1 的 app 不用堆）。目标是
  链接符号 `_am_heap_start` 到 `$0800`（`mos6502-nes`）；`malloc` 兼容层仍属可选项，
  **不属于 M1**。

### 6.4 Host 端口

| 端口 | 方向 | 平台 | 语义 |
|---|---|---|---|
| `$F001` | 写 | nemu | 向宿主日志/控制台输出一个字符 |
| `$F002` | 写 | nemu | 以该字节为状态退出仿真 |
| `$F003` | 读 | nemu | 读一个字符（EOF 结束仿真） |
| `$4018` | 写 | nes | 向宿主输出一个字符 |
| `$4019` | 写 | nes | 以该字节为状态退出仿真 |

`_putc(c)` → `$F001`（nemu）或 `$4018`（nes）。`_halt(code)` → `$F002`（nemu）或
`$4019`（nes），随后 `SEI; JMP *` 自旋。真机上这两个写落在未用寄存器上，程序就
自旋——对裸机 `halt` 来说这就是正确行为。

**实现现状（M2）：** 两个宿主已解码，bench 已观察：

1. `NEMU/src/nes/nes.c`——`nes_cpu_write()` 把 `$4018` 变成字符、把 `$4019` 变成
   带状态的 `NEMU_END`；
2. `GEM6502/src/gem6502.c` + `main.c`——同样的端口与停机语义，进程退出码跟随停机码；
3. `tb/open6502_am6502_tb.sv`——**不用改 RTL**：盯
   `dut.u_cpu.bus_we && dut.u_cpu.bus_addr == 16'h4018/16'h4019`，打印字符，停机码
   非零或画面全黑就判失败。

`rtl/bus/open6502_bus.sv` + `rtl/dbg/open6502_difftest_bridge.sv` 仍未解码这两个端口，
只有在 FPGA/调试探针需要时才做。

### 6.5 退出状态

退出码取 `_halt` 参数的低 7 位（第 7 位在某些宿主上与 open bus 无法区分）。
`0` = 通过，非零 = 携带该码失败。跑到指令预算仍未 `_halt` 的，算**失败**而不是
通过：`make run` 必须报 `TIMEOUT`，与 Klaus harness 的判定一致。

### 6.6 每个 app 的成功判据

不是每个 app 都能调用 `_halt`：Klaus 靠在一个固定 PC 上自旋报成功，而它是本仓库
不参与汇编的外部二进制。因此 `apps/<app>/app.mk` 必须声明一个判据：

| `CHECK` | 通过条件 | 失败条件 |
|---|---|---|
| `halt`（默认） | host 退出端口被写入 0 | 退出端口写入非零，或指令预算耗尽 |
| `pc:<addr>` | PC 到达 `<addr>` 并停在该处 | 预算耗尽时 PC 停在别处 |

`tools/run.py` 从 app 本身读判据，而不是从 shell 变量读，所以同一条 `make test`
对一个编译出来的 C 程序和 Klaus 的预编译 `.bin` 含义一致。预算耗尽永远不算通过。

## 7. IOE 设备表

`am.h` 保留 AM 的形状：

```c
size_t _io_read (uint32_t dev, uintptr_t reg, void *buf, size_t size);
size_t _io_write(uint32_t dev, uintptr_t reg, const void *buf, size_t size);
```

`dev` 号沿用 AM 的魔法常量（`am/amdev.h`），所以同一个 `_DEV_*` 在两边含义一致；
AM 里没有对应物的设备放在私有的 `0x0000ac1x` 段，并明确标注为扩展。未实现的
`(dev,reg)` 返回 0——不 trap、不 abort，因为目的就是让一个 app 能编给多个平台。

| dev | 名字 | regs | 说明 |
|---|---|---|---|
| `0x0000ac02` | `_DEV_INPUT` | 1 = 1P，2 = 2P | 位序 = NES 手柄移位寄存器顺序（`A B Select Start Up Down Left Right`） |
| `0x0000ac03` | `_DEV_TIMER` | 1 = `UPTIME`（u32 帧计数） | `_am_frames`，由平台 tick 递增 |
| `0x0000ac05` | `_DEV_SERIAL` | 2 = `SEND`（效果同 `_putc`），1 = `RECV`（返回 0） | 串口就是宿主控制台；`_putc` 不是魔法，它就是 `SERIAL_SEND` |
| `0x0000ac10` | `_DEV_PPU`（扩展） | 1-8 = `$2000-$2007`，9 = 触发 OAM DMA | 直通读写；PPU 的怪异行为由 app 自己负责 |
| `0x0000ac11` | `_DEV_APU`（扩展） | 1 = `$4000-$4017` 窗口 | 非 NES 平台返回 0 |
| `0x0000ac12` | `_DEV_CART`（扩展） | 1 = mapper 号，2 = bank，3 = mirroring，4 = IRQ 应答 | 让测试能问“我是哪个构建” |

故意不做：`_DEV_VIDEO`（PPU *就是*帧缓冲；通用帧缓冲抽象会藏掉这个项目正
想验证的扫描线行为）、`AM_DISK`、`AM_SDCARD`、`AM_AUDIO`（内环不需要宿主音频）。

## 8. 构建与运行接口

```sh
cd labs/am6502
make ARCH=mos6502-nemu APP=hello                # 平坦镜像
make ARCH=mos6502-nes  APP=hello                # 卡带镜像 + .hex
make ARCH=mos6502-nes  APP=hello run DUT=rtl    # 同一镜像走 RTL bench
make test                                       # 矩阵
```

| 命令 | 产出 / 行为 |
|---|---|
| `make ARCH=... APP=...` | `build/<app>-<arch>.{bin 或 nes, elf, map, lst, sym}` |
| `make run ARCH=... APP=... DUT=nemu` | `6502EMU -b -a 0 -r`（平坦）或 `6502EMU`（nes）；判定来自 `_halt` 或 `CHECK=pc:...` |
| `make run ... DUT=gem` | `gem6502 game <rom> <frames> <ppm>`（仅 nes） |
| `make run ... DUT=rtl` | 用 `tb/` 的顶层跑 `iverilog`/`verilator`，装载派生的 hex，观察 `$4018` |
| `make test` | 每个 app 在每种支持的 DUT 上构建并运行；app 的 `CHECK` 判据不成立即失败 |
| `make clean` | 只删 `build/`，不碰 `NEMU/`、`GEM6502/`、`rtl/` |

环境变量：

| 变量 | 默认值 | 含义 |
|---|---|---|
| `AM6502_HOME` | `labs/am6502` | AM 树的根（对应 AM 的 `AM_HOME`） |
| `OPEN6502_ROOT` | 往上两级 | `NEMU/`、`GEM6502/`、`rtl/`、`tb/` 所在处 |
| `NEMU` | `$OPEN6502_ROOT/NEMU/build/6502EMU` | 参考模型可执行文件 |
| `GEM6502` | `$OPEN6502_ROOT/GEM6502/gem6502` | 周期模型可执行文件 |
| `MAPPER` | `0` | `DUT=rtl` 时 `open6502_top` 的参数 |
| `MAX_INST` | 每 app 自带 | 指令预算；超时即失败 |
| `VERBOSE` | `0` | 在 `build/logs/` 保留每次运行的日志 |

M1 实现的是 `AM6502_HOME`、`OPEN6502_ROOT`、`NEMU` 和每个 app 的
`APP_MAX_INST`；表里其余行是目标，不是现状。

平坦镜像一律用 `-a 0` 装载；6502EMU 默认的 `$8000` 会把 64 KiB 镜像放错位置。
这是 AM 式“镜像即机器”约定唯一需要额外一个 flag 的地方。

失败信息必须说清是哪一段失败，而不只是哪个程序：

```text
FAIL hello-mos6502-nes / rtl: timeout after 20,000,000 cycles, last PC=$C0A2
  exit port: never written; first dev write: $2000=0x3f at cycle 812
```

## 9. 仓库布局

```text
labs/am6502/
├── Makefile                  # 顶层：apps x arches、run、test、clean
├── README.md / README.zh-CN.md
├── am/
│   ├── Makefile              # 每个 ARCH 产出 libam.a（对应 AM 的 am/Makefile 形状）
│   ├── am.h                  # 契约：TRM + IOE 子集
│   ├── include/amdev.h       # dev/reg 编号、_DEV_INPUT 位序
│   ├── arch/                 # 每个 ARCH 一个 .mk，列出 AM_SRCS（对应 AM 的 arch/*.mk）
│   │   ├── mos6502-nemu.mk
│   │   └── mos6502-nes.mk
│   └── src/
│       ├── common/           # string/mem 辅助、基于 _putc 的 printf-lite、am_mainargs
│       ├── nemu/             # crt0、$F001-$F003 驱动、平坦链接脚本
│       └── nes/              # crt0、$4018/$4019、NROM 链接脚本
├── apps/
│   ├── hello/                # 输出一行、以 0 退出——冒烟测试
│   ├── klaus/                # 下载外部二进制、补向量，不编译
│   └── nestest/              # 指令 trace 测试：第一个有真实输出量的 app
├── tools/
│   ├── ines.py               # .nes <-> prg/chr hex，头部校验（取代写死的 /tmp 路径）
│   ├── mkimage.py            # 平坦镜像拼装、向量校验、可复现字节序
│   ├── run.py                # DUT 分派 + 失败格式化
│   ├── difftest.py           # 跑 NEMU+RTL trace、比对、首个分歧报告
│   └── difftrace.py          # trace 比对器（P 掩码、长度检查）
├── docs/
│   ├── design.md             # 英文设计文档
│   └── design.zh-CN.md       # 本文
└── .gitignore                # build/、obj/、logs/
```

`tools/ines.py` 应当吸收 `sim/game/extract_ines.py` 而不是再抄一份；这样 RTL 游戏
bench 读到的镜像，和其他所有 app 走的是同一条代码路径。

## 10. 与 DiffTest、CI 的接口

这才是这个 lab 存在的理由。与
[docs/open6502-chip/difftest.md](../../../docs/open6502-chip/difftest.md) 的契约：

- **一个镜像，两台机器**：`build/hello-mos6502-nemu.bin` 带 `NEMU_TRACE_STATE` 跑
  `6502EMU -a 0 -r`；同样的字节**原样**装进 `tb/open6502_am6502_flat_tb.sv`，带
  `+TRACE` 跑 RTL。两边都按“每条已退休指令一行”（`PC A X Y SP P`）输出，
  `tools/difftrace.py` 比对（P 去掉 B/U 位）并打印首个分歧及上下文；
  `make difftest` 把整条链串起来。
- **一个镜像，帧哈希**：`hello-mos6502-nes.nes` 在 `6502EMU --hash` 与 RTL `tb`
  的帧哈希；不一致时打印第一个不同的帧，而不是一句 `FAIL`。
- **一个种子**：v0 只有 `mainargs` 一个输入旋钮；没有墙钟，没有未初始化 WRAM
  （crt0 清运行时零页和 RAM 区，所以 `main` 之前 `.bss` 已经是 0）。

CI 是 `.github/workflows/am6502.yml`（随 M3 加入）：

```text
make -C labs/am6502 test ARCH=mos6502-nemu        # 不需要 RTL，不需要 iverilog
make -C labs/am6502 test ARCH=mos6502-nes DUT=nemu
make -C labs/am6502 test ARCH=mos6502-nes DUT=gem
make -C labs/am6502 test ARCH=mos6502-nes DUT=rtl # 需要 iverilog
make -C labs/am6502 difftest ARCH=mos6502-nemu    # NEMU 与 RTL 逐指令比对
```

第一行才是关键：它能在没有 Verilog 工具链的笔记本上变绿，这才让这条门禁在只改 C
的贡献者手里活得下来。

## 11. 里程碑

| 里程碑 | 范围 | 验收条件 | 粗估 |
|---|---|---|---|
| **M0** | 本文档 + 目录 + 索引条目 | 评审通过，不声称任何代码 | 随本次提交完成 |
| **M1** | `mos6502-nemu` 打通：`hello`、`klaus` 包装、`tools/run.py` | `make test ARCH=mos6502-nemu` 在 `6502EMU` 上全绿；Klaus 在 `PC=$3469` 通过，且**用的是构建自己的 reset 向量**——**2026-11-16 已落地** | 1-2 周 |
| **M2** | `mos6502-nes`：iNES 产出、`ines.py`、tb 观察 `$4018/$4019`、NEMU NES 模式解码 | 同一个 `hello.nes` 跑通 `6502EMU`、`gem6502`、RTL，画面非空，退出码 0——**2026-11-16 已落地**（3/3/4 帧，整屏白） | 2-3 周 |
| **M3** | DiffTest + CI：逐指令比对、失败报告、workflow | `make difftest APP=hello` 打印首个不同的指令；CI 跑 §10 的几行——**2026-11-16 已落地**（1,216 条指令一致；Klaus 局部 20 万条一致） | 2 周 |
| 加分 | `mos65c02-nemu`、`nes-mmc3`（mapper 4）、`printf`/`malloc` 兼容层、把 `NEMU/nesgame` 迁到同一套 CRT | 各自独立有用，互不阻塞 | - |

M1 故意只做 NEMU：不需要新 RTL、不需要新仿真器功能，也不需要“除了能汇编 6502 并
补一个向量”之外的任何工具链假设。如果 M1 不能很快变绿，后面整个设计都值得怀疑。

## 12. 风险与待决问题

| # | 风险 / 问题 | 缓解 | 要先做 spike？ |
|---|---|---|---|
| R1 | **工具链**：llvm-mos（`mos-nes-nrom`）还是 ca65。仓库已经在 `NEMU/nesgame/build.sh` 用 `ca65/ld65`；llvm-mos 能写 C 但安装重 | v0 只发汇编 app（`hello.s`、`nestest`、Klaus 包装），C 支持是升级项而不是依赖 | 是——1 天：用 llvm-mos 编 `hello.c`，量 PRG 大小 |
| R2 | 32 KiB PRG / 2 KiB WRAM 很紧，C 运行时可能吃掉预算 | CRT 保持最小，CI 里量 `.map`，数字和 `jit6502` 的基准表并列公布 | 否——M1/M2 里量 |
| R3 | 在 NEMU 里解码 NES 模式的 `$4018` 是改黄金参考 | 只在 host 侧、加开关，不改 CPU/PPU 时序；或者 M3 前只在 tb 观察 | 否 |
| R4 | RTL 顶层目前由 bench 固定实例化 mapper 0 | `MAPPER` 参数已存在；UxROM/CNROM/AxROM 只需 bench 参数，不用改 RTL | 否 |
| R5 | nexus-am 没有许可 | 不复制任何东西；边界记录在 §2 | 否 |
| R6 | 命名：`am6502` 可能被误读为 AM 兼容 | README 写明“精神上接口一致，不是移植”；改名很便宜 | 否 |
| R7 | 是否与 `NEMU/nesgame/build.sh` 重复？ | nesgame 变成 `apps/` 的一个条目（`combine.py` 并入 `tools/ines.py`），不允许第二套构建系统存活 | 否 |

M1 之前值得先定的一件事：**`mos6502-nes` 的 `_putc` 要不要写 PPU 文本控制台，而
不是 `$4018`？** 控制台能让 nestest/blargg 的输出在*真机*和任何不做 host 补丁的
仿真器上可读，但需要等 vblank、保存滚动（约 150 行和一个新的失败模式）。建议：
v0 用 `$4018`，PPU 控制台作为加分项里的 `nes-console` 平台。

## 13. 致谢与出处

- Abstract Machine 接口（`_putc`、`_halt`、`_io_read`、`_io_write`、
  `ARCH=<isa>-<platform>`、`mainargs`、`AM_HOME`）是 AM 项目（NJU PA /
  `nexus-am`，OpenXiangShan）的设计。本 lab 只重新实现 6502 能支持的子集，不复制
  源码，详见 §2。
- NES 内存图、iNES 容器与 mapper 行为遵循 `NEMU/src/nes/` 和 `rtl/cart/` 已经在用
  的公开 NES 文档。
- Klaus、nestest、blargg 测试 ROM 仍归各自许可所有，**只下载、不入库**——与
  `roms/LICENSES` 对随仓库分发 ROM 的规则一致。
