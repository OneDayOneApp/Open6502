[English](README.md) | **简体中文**

# am6502：给 6502/NES 的一个小抽象机

**状态：M1 可用。** `mos6502-nemu` 平台已经能构建、运行、自检：`make run` 在黄金
参考模型上打出 `hello from am6502` 并以 0 退出；`RUN_KLAUS=1 make test` 跑通 Klaus
功能测试，而且**用的是构建自己写出的 reset 向量**，不是 testbench 事后补的。

这是 [nexus-am](https://github.com/OpenXiangShan/nexus-am) 思路的 6502 子集：一层
可以链接的抽象、一个 `make ARCH=<isa>-<platform>` 矩阵、一套所有后端都吃的镜像
约定。

```sh
cd labs/am6502
make                      # 产出 build/hello-mos6502-nemu.bin（及 .hex/.map/.lst）
make run                  # 6502EMU -a 0 -r：PASS，退出 0
RUN_KLAUS=1 make test     # hello + Klaus（默认下载约 64 KiB，可用 KLAUS_BIN 指定本地副本）
```

依赖：`ca65`/`ld65`（cc65）、`python3`，以及已构建的 `NEMU/build/6502EMU`。

## 为什么需要它

仓库此前有三条装载路径，没有共同契约：

| 路径 | 镜像 | 装载方式 |
|---|---|---|
| Klaus 测试 | `klaus.hex`，reset 向量由 bench 改写 | `tb/open6502_klaus_tb.sv` |
| 游戏画面 | `/tmp/falling/*_prg.hex`、`*_chr.hex` | `tb/open6502_game_tb.sv` |
| DiffTest / 单元 | `sim/difftest/difftest.bin` | `6502EMU -b --load-addr=...` |

锁步 DiffTest 要求两边是**同一个镜像**，CI 门禁要求**每个程序一条命令**。
`am6502` 是把这两件事对 6502 规模程序变成现实的最小方案。M1 的镜像是逐字节可复现
的（构建两次，SHA-256 相同），这是 golden 哈希能成立的前提。

## 镜像契约

| ARCH | 镜像 | 入口 | Host 输出 / 退出 | 后端 |
|---|---|---|---|---|
| `mos6502-nemu`（M1） | 平坦 65,536 字节 `.bin` + 派生 `.hex` | `$FFFC` 的 reset 向量，由构建写出 | `$F001` / `$F002`（6502EMU 已有端口） | `6502EMU -a 0 -r` |
| `mos6502-nes`（M2） | iNES `.nes` + 派生 `.prg.hex`/`.chr.hex` | reset 向量位于最后一个 PRG bank | `$4018` / `$4019`（仅 host 侧） | `6502EMU`、`gem6502`、RTL `tb` |

每个 app 自己声明“什么算成功”，所以 `make test` 对编译程序和预编译 `.bin` 含义
一致：

| `CHECK` | 通过 | 失败 |
|---|---|---|
| `halt`（默认） | host 退出端口写入 0 | 写入非零，或指令预算耗尽 |
| `pc:0x3469` | PC 到达该地址并停下 | 预算耗尽时停在别处 |

## 目录

| 路径 | 用途 |
|---|---|
| `am/am.h`、`am/include/amdev.h` | 契约：TRM + IOE 子集、`_DEV_*` 编号 |
| `am/arch/mos6502-nemu.mk` | 平台：镜像类型、装载地址、源码清单 |
| `am/src/nemu/{crt0.s,trm.s,flat.cfg}` | 启动、host 端口、ld65 布局 |
| `apps/hello/` | 冒烟测试：`_putc` + `_halt(0)` |
| `apps/klaus/` | 把外部功能测试包装成镜像（`CHECK=pc`） |
| `tools/mkimage.py` | 补齐到 64 KiB、写入/校验 reset 向量、导出 hex |
| `tools/run.py` | DUT 分派、`CHECK` 判据、失败诊断 |
| `docs/design.zh-CN.md` | 完整规格、里程碑、风险 |

## M1 做了什么、没做什么

| 已完成 | 尚未做 |
|---|---|
| `mos6502-nemu` 平台、crt0 + TRM、可复现镜像 | `mos6502-nes`（iNES、`$4018/$4019`、PPU/APU） |
| `hello` 与 Klaus 走通 `make run` / `make test` | C 工具链（llvm-mos）、`mainargs`、`_heap` |
| `_putc`、`_halt`、`_io_read`/`_io_write` 桩、`am_mainargs`（返回 ""） | 真实 IOE 驱动（输入、定时器、PPU、卡带） |
| `.hex`、`.map`、`.lst` 产物 | `.elf`/`.sym`、`DUT=gem`、`DUT=rtl`、锁步 DiffTest、CI |

`_io_read`/`_io_write` 返回 0 而不是 trap——这是设计里“一个 app、多个平台”的规则；
等 NES 平台接上真实设备再谈语义。

## 接下来读

- [docs/design.zh-CN.md](docs/design.zh-CN.md) —— 中文设计规格。
- [docs/design.md](docs/design.md) —— 英文设计规格。
- [../../docs/open6502-chip/difftest.md](../../docs/open6502-chip/difftest.md) ——
  DiffTest 一侧必须满足什么。
- [../jit6502/README.zh-CN.md](../jit6502/README.zh-CN.md) —— 同门 lab，同样
  “独立实验”的规矩。
