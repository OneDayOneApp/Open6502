# NES/Famicom Golden Reference（基于 NEMU）

本目录定义以 NEMU 为基础的完整 NES/Famicom 芯片级 golden reference 设计方案、
测试方案和验收 CheckList。

- [设计方案](design.md)
- [测试方案](test-plan.md)
- [CheckList](checklist.md)

## 一句话定义

Golden reference 是 **NEMU 的一个专用 REF 构建**：

- 复用 NEMU 的 CPU 执行引擎、ISA 框架、内存、监视器、trace 和 DiffTest 基础设施；
- 在 NEMU 内增加 2A03/2C02/APU/总线/mapper 的 NES 平台；
- 生成可执行 REF 和 `TARGET_SHARE` REF `.so`；
- DUT（RTL、另一 NEMU 构建或其他模拟器）通过 NEMU DiffTest 及其 NES 扩展协议
  与 REF 对比；
- 当前 `6502EMU` 前端是原型/smoke test 平台，不是 golden reference。

## 关键原则

1. **NEMU 是底座**：不在 NEMU 之外另起一套独立模拟器。
2. **REF/DUT 分离**：REF 与 DUT 不能是同一构建配置。
3. **先 CPU，后 PPU/APU**：CPU 复用 NEMU DiffTest；PPU/APU/mapper 用扩展协议。
4. **确定性**：master tick、状态快照、hash、trace 全部纳入 NEMU REF。
5. **实机优先**：参考模拟器只做交叉检查，不能替代硬件采集和规范。
6. **边界清晰**：T0+T1 是默认“完整”，T2/T3 单独验收。

## 最快落地路径

1. 新增 `m6502-nesref_defconfig` 和 `m6502-nesref-so_defconfig`。
2. 在 `src/nes` 内把总线、PPU、APU、mapper 拆成可被 NEMU monitor/REF 调用的模块。
3. 给 `src/isa/m6502` 增加 2A03 profile、总线周期和 CPU DiffTest。
4. 扩展 `difftest-def.h` 和 REF 导出接口。
5. 先让 CPU-only DiffTest 跑通 Klaus/nestest，再逐级加入 PPU/APU/mapper。
6. 最后用实机采集和自由 ROM 回归验收。

## 当前进度（TDD）

已落地并纳入 `NEMU/tests/run.sh`：

- `src/nes/ref/clock.c`：NTSC/PAL master tick、CPU/PPU 分频、帧边界。
- `src/nes/ref/rom.c`：iNES/NROM 头解析、PRG/CHR/CHR RAM。
- `src/nes/ref/state.c`：确定性 hash。
- `src/nes/ref/machine.c`：2KB RAM 镜像、PRG RAM、NROM 总线、flat test bus。
- `src/nes/ref/cpu.c`：从 NEMU m6502 核移植的 REF CPU 后端。
- `tests/nes_ref/`：单元测试、基础 CPU 程序、可选 Klaus。
- `tests/difftest/`：REF `.so` + DUT 的 CPU-only DiffTest。
- `configs/m6502-nesref-so_defconfig` / `m6502-nes-dut_defconfig`。

验证结果：

```text
PASS: nes_ref clock/rom/hash
PASS: REF CPU Klaus at PC=3469 (cycles=96241364)
PASS: NEMU CPU-only DiffTest
```

尚未完成：NEMU REF/DUT 专用配置、`isa_difftest_checkregs`、逐 dot PPU、
完整 APU、NES 扩展 DiffTest、实机对齐。当前进度是 M0/CPU 基座，不是完整 REF。
