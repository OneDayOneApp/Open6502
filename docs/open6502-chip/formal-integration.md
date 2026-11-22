# 形式化验证与 RTL 的集成方式

> 状态：已决定（2026-11-22）。适用范围：[formal/](../../formal/README.md) 的 Lean 模型与
> [rtl/cpu/open6502_cpu.sv](../../rtl/cpu/open6502_cpu.sv)。
> 相关证据：[verification.md](verification.md) 的 L8 层、[scorecard-history.md](../scorecard-history.md)
> 的 2026-11-22 记录。

## 1. 结论

- **不以“导出 SystemVerilog 替换现有 RTL”为主路线。**
- **以“可执行规格 + 模拟器/oracle 模式参与 difftest”为主路线。**
- SV 导出只用于**新叶子模块**或**生成化**路线；不用它重写已经工作的 CPU/SoC。

一句话：形式化负责子集的静态正确性，difftest 负责全系统的动态覆盖；两者正交，
形式化的产出应以 oracle、断言和一致性门禁的形式进入 CI，而不是替代 difftest。

## 2. 背景与约束

当前证明对象有两层：

1. Lean 的 ISA 规格（`Open6502/ISA.lean`）与 Lean 的 FSM 模型（`Open6502/Micro.lean`，
   逐寄存器对应 `rtl/cpu/open6502_cpu.sv` 的转写）；
2. `mrun n m = step m`：19 条指令的逐周期精化，以及全译码一致性、ADC/SBC（含 BCD）
   全输入证明、15 个程序测试、4 个程序的 Lean↔RTL 协同仿真。

因此：

- 证明证明的是“Lean 模型实现 Lean 规格”，**不是**“SystemVerilog 实现规格”；
- 现有 RTL 是产品，包含 Lean 未建模的 SoC 内容：总线/PPU/APU/mapper/DMA、时钟域、
  复位、时序与非法指令行为；
- 直接换源会丢失这些系统行为，而“转写是否正确”这个 transcription gap 仍需
  仿真或等价检查来闭合。

## 3. 两条路线

### A. 直接导出 SystemVerilog（CktLean 风格）

做法：以 Lean 为唯一源，`#synthesizeVerilog` 导出 SV，再做综合/仿真/等价。

适用的场景：

- 新写一个叶子模块（ALU、译码、计数器、局部 FSM），从零开始且接口简单；
- 走“生成化”维度（当前 0 分）：参数化生成 + 与手写版等价回归；
- 教学/研究：完整复现 CktLean 的“规格 → 生成 → 证明 → 物理设计”闭环。

不适用的场景：

- 替换现有 `open6502_cpu.sv`：SoC 集成与系统时序不在模型里；
- 需要保留手写优化的模块；
- 把“导出成功”当成“硬件正确”——后端仍要仿真、综合与等价检查。

### B. 模拟器/oracle 模式参与 difftest

做法：Lean 模型可执行，直接参与回归：

1. 作为**第三方参考**，与 NEMU、RTL 在同一 flat image 上逐指令比对；
2. 作为**断言源**：把精化定理里的周期数、标志语义变成 RTL 仿真的断言；
3. 作为**规格门禁**：译码表与 RTL 分派的一致性检查（已落地）。

优点：成本低、直接保护现有 RTL；把“形式化规格”变成每天都会跑的回归。
缺点：仍是抽样，不是全输入证明；但与精化定理互补。

## 4. 决策

- **主路线 B**：现有 CPU 保持手写 SV，Lean 作为可执行规格、oracle 与断言源；
- **路线 A 限定使用**：只用于新的叶子模块和生成化尝试；
- 两者共享同一份 Lean 源，避免“证明一套、仿真另一套”。

## 5. 落地顺序

1. **规格 oracle（已落地）**
   - `Open6502/Refinement.lean`：19 条指令的 `mrun n m = step m`；
   - `scripts/check_rtl_sync.py`：译码表 ↔ 真实 SV 分派，进 `lean.yml`；
   - `scripts/cosim.py`：4 个程序的 Lean ↔ RTL 最终状态比对。
2. **三方 difftest（下一步）**
   - 同一 flat image 跑 NEMU / RTL / Lean，逐条比对 `pc a x y p sp`；
   - 复用 `labs/am6502` 的 difftest 协议与 `open6502-run --trace` 的轨迹格式；
   - 首差异报告带指令上下文；作为 `lean.yml` 的新 job。
3. **随机指令一致性**
   - 对 19 条已精化指令随机操作数/内存/标志，三方比对；
   - 重点覆盖 BCD、标志位、零页与页回绕边界。
4. **周期数断言**
   - 精化定理给的是 FSM 周期数；在 RTL 仿真中对这批指令加周期断言，
     验证转写的周期序列。
5. **长期：闭合 transcription gap**
   - 对实际 SV 与 Lean FSM 做等价检查（Yosys/ABC 或 Verilator 有界等价）；
   - 或对生成化的新模块做“Lean 源 → SV → 等价证明 + difftest”。

## 6. 判定标准

“形式化参与了 RTL 改进”至少满足：

- CI 中三方 difftest 为绿，并对注入的 RTL bug 能报出首个分歧；
- 精化覆盖从 19 条继续提升（官方 151 条、IRQ/NMI 入口、复位序列）；
- 随机一致性与周期断言覆盖 BCD、标志、寻址边界；
- 若走生成化：生成的 SV 与手写版等价，且 difftest 通过。

## 7. 明确不做

- 不用形式化证明替代 difftest；
- 不把 Lean 模型当作唯一源去重写已经工作的 CPU/SoC；
- 不把“19 条指令精化”表述为“整个 CPU 已验证”；
- 不把“导出 SV 成功”当作后端正确性的证据。
