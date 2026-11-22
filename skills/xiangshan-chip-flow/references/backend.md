# 后端：综合、FPGA、ASIC 与 PPA 口径

> 后端不是“把 RTL 跑一遍工具”，而是把**可解释的代价**变成**可评审的证据**。先统一口径，再看数字。

## 1. 综合流程（Yosys 示例）

`[事实]` Open6502 的脚本：

```sh
yosys -s rtl/scripts/synth_open6502.ys
```

脚本内容：

```text
read_verilog -sv -DSYNTHESIS rtl/open6502_pkg.sv
read_verilog -sv rtl/...（各模块）
hierarchy -check -top open6502_top
synth -top open6502_top -flatten -noabc
stat -tech xilinx
```

逐模块面积报告则用：

```text
read_verilog -sv -DSYNTHESIS
hierarchy -check
synth -top <module> -flatten -noabc
stat
```

### 1.1 口径必须写清

| 口径 | 含义 | 不能拿来做什么 |
|---|---|---|
| generic cells | Yosys 通用单元计数 | 不能当 FPGA LUT、ASIC µm² |
| LUT/FF | 映射到具体 FPGA 后的资源 | 不能当 ASIC 面积 |
| µm² | 标准单元 + 宏的实际面积 | 未 PnR 前不是签核面积 |
| 逐模块之和 | 各模块独立综合相加 | 不等于全芯片 flatten 结果 |
| 全 top flatten | 真实层级/优化后的结果 | 可能跑不完/被工具优化 |

### 1.2 `-noabc` 的代价

Open6502 用 `-noabc` 是因为 PPU 组合渲染器太大，ABC 映射在实用时间内跑不完。代价：

- 面积数字偏“逻辑膨胀”，不是最终映射。
- 时序/Fmax 不可信。
- 必须明确写“这是 generic-cell 估计，不是签核”。

### 1.3 综合差异必须记录

- `SYNTHESIS` 跳过存储器初始化 → 仿真有初值，综合没有。
- `SYNTHESIS` 限制 sprite 扫描 8 个 → 仿真扫描 64 个，行为不同。
- 综合裁掉 `dbg_*` → 可观测性下降。
- 综合后必须回跑同一套仿真/门禁；不能“综合过 = 正确”。

## 2. 面积归因（先找主因，再优化）

`[事实]` Open6502 的 Yosys 面积：

| Block | Generic cells |
|---|---:|
| CPU | 4,138 |
| APU + DMC | 2,683 |
| Mapper | 745 |
| OAM DMA | 154 |
| WRAM | 37,003 |
| Bus/clock/video/controller | 779 |
| PPU | 975,919 |
| **Total** | **1,021,421** |

PPU 内部：84,224 FF、840,701 mux、40,283 AND、9,301 OR、674 XOR。主因是 CHR/VRAM/OAM/palette 被展开成 FF + 大 mux 树，不是“CPU 太大”。

**优化顺序**：

1. 大存储 → BRAM/SRAM 宏（单口/简单双口、同步读）。
2. 组合扫描 → 顺序扫描/分拍，减少 mux 树。
3. 宽 mux → 分级/译码。
4. 最后才做逻辑最小化/门级优化。

`[事实]` 55nm 规划估算：当前 FF 存储 4–7 mm²；SRAM + PPU 重构 1.2–2.0 mm²；规划目标 ~1.5 mm²。单位成本 ~USD 0.3–0.7，mask/NRE ~USD 150k–400k+。**这些是规划估算，不是报价/签核。**

## 3. FPGA 门禁

### 3.1 必做

```text
[ ] 全 top 综合完成（不是逐模块）
[ ] BRAM 推断（CHR/VRAM/OAM/palette）
[ ] 时序约束（clock/create_clock/IO delay/false path）
[ ] Fmax 报告 + 关键路径
[ ] PLL/MMCM + 复位释放
[ ] 视频/音频/输入引脚约束
[ ] ROM/Flash/SD 装载
[ ] 板级 bring-up
[ ] 连续运行 ≥1 小时
[ ] 无“仿真专用假设”（初始化/延迟/文件 IO）
```

### 3.2 常见坑

- 存储器没推断成 BRAM → 资源爆炸/时序崩。
- 多周期路径没约束 → 假 Fmax。
- 异步输入没同步 → 偶发挂死。
- 复位释放没对齐时钟 → 上电不稳定。
- 只跑仿真帧，不做板上视频/音频验证。
- 用 JTAG 改寄存器当“功能验证”，不复跑门禁。

## 4. ASIC/PDK 门禁

### 4.1 流程

```text
RTL freeze
  → 标准单元库/SRAM 宏
  → DFT/scan 插入 + 覆盖率
  → clock/reset/power intent（UPF/CPF）
  → 综合（目标库）
  → STA（多 corner）
  → PnR + CTS + 布线
  → DRC/LVS/ERC
  → 功耗/IR drop/EM
  → pad ring/ESD/package
  → tapeout checklist 评审
```

### 4.2 退出条件

- timing 全 corner 收敛（setup/hold），无未约束路径。
- DFT 覆盖率达标，scan chain 可测。
- DRC/LVS/ERC 全清。
- 功耗/IR/EM 在预算内。
- pad/ESD/package 评审通过。
- shuttle/资金落实。
- **没有口头“流片就绪”**。

### 4.3 仓库边界

- 不提交 foundry PDK、SRAM compiler 输出、专有标准单元库。
- 只提交脚本/接口/约束模板，用户自备 PDK 运行。
- 第三方 IP/ROM 保留许可证与来源；分发前审核兼容性。

## 5. PPA 报告模板

```markdown
# <chip> PPA Report

## Scope
- top/module:
- RTL commit:
- tool version:
- library/PDK:
- constraints:
- corner:

## Area
- total µm² / LUT-FF / BRAM:
- top blocks:
- memory macros:
- utilization:

## Timing
- target frequency:
- WNS/TNS:
- critical path:
- clock domains:

## Power
- dynamic/leakage:
- activity assumptions:

## Caveats
- what is estimated vs signed off:
- what is not covered:
```

## 6. 生成化（Chisel/FIRRTL）扩展

`[事实]` Open6502 没有生成化，这是九维里的 0 分项。新芯片若规模大/参数多，建议补，但顺序不能反。

### 6.1 正确顺序

1. REF + DiffTest + CI 已绿。
2. 冻结参数化接口（位宽/端口/配置）。
3. 选叶子模块（ALU/译码/子模块）用 Chisel 重写。
4. 与 SV 版 co-sim 等价（同输入/同输出/同周期）。
5. 等价后扩大生成范围。
6. 生成物纳入同一 lint/sim/difftest 门禁。

### 6.2 反模式

- 全盘重写后再补验证 → 两个都不可信。
- 生成器参数无边界检查 → 配置爆炸。
- 生成物不可读/不可调试 → 定位不到首个差异。
- 只做“能生成”不做等价 → 生成化分数虚高。

## 7. 成本模型（只做规划）

`[事实]` Open6502 的规划假设：

```text
NAND2 equivalent: 2.0-3.0 um^2
DFF: 10-15 um^2
6T SRAM bit: 0.35-0.55 um^2
std-cell utilisation: 60-70%
IO/PAD/ESD: 0.3-0.8 mm^2
300mm usable wafer: ~70,000 mm^2
55nm wafer: USD 2,500-4,500
yield: 70-85%
```

用途：判断“SRAM 重构值不值得”“die 预算够不够”，**不是报价**。真实数字必须来自 PDK + 代工/OSAT 报价。

## 8. 后端评审问题清单

1. 这个面积/时序数字是哪种口径？
2. 存储是 FF 还是 BRAM/SRAM？推断成功了吗？
3. 约束覆盖了哪些时钟/IO/多周期/false path？
4. 综合后回跑了哪些门禁？
5. 关键路径在哪？是逻辑还是布线？
6. DFT/DRC/LVS 清了吗？
7. 有哪些“仿真能过、板上不一定”的假设？
8. 如果明天流片，最可能死在哪一项？
