# RTL 与仓库规范

> 目标：让“改一行 RTL”不会引入不可观测、不可综合、不可复现的债。示例来自 Open6502，换芯片时替换命名与领域细节。

## 1. 目录与命名

```text
rtl/
  <chip>_pkg.sv            常量/类型/枚举
  <chip>_top.sv            唯一 SoC top
  cpu/ bus/ <device>/ ...  一个 block 一个目录
  fpga/                    板级 wrapper
tb/                        testbench（每个特性一个）
sim/difftest/              锁步比对 runner
docs/spec/                 brief/architecture/difftest/verification/checklist
docs/adr/                  不可逆决策
```

命名：

- 文件/模块：`<chip>_<block>.sv`，小写下划线；一模块一文件。
- 时钟：`clk_<freq>`；使能：`<domain>_en`；复位：`rst_n` / `<domain>_rst_n`。
- 总线：`bus_addr/bus_wdata/bus_rdata/bus_we/bus_strobe`。
- 调试：`dbg_*`，只读观测，不参与功能。
- 参数：`parameter` 用于结构/映射；频率/分频/分辨率放 package。

## 2. 时钟与复位

`[事实]` Open6502 用单 21.477272 MHz master + 相位使能：

```systemverilog
cpu_en  // master/12
ppu_en  // master/4
aud_en  // 44.1 kHz strobe
```

规则：

- 所有时序逻辑跑同一个 master clock；跨域用 enable 相位，不造第二时钟。
- 复位异步 assert、同步 release（两级同步器）；释放后所有使能相位确定。
- 不在模块内部生成时钟；PLL/MMCM 只在 top/板级 wrapper。
- 时钟门控若使用，必须有 DFT 可测方案；仿真与综合语义一致。
- 跨域信号必须单比特/握手/异步 FIFO；禁止裸多比特直连。
- 复位值要写清；仿真初始化不能依赖“上电为 0”。

## 3. 常量与类型

`[事实]` `open6502_pkg.sv` 定义 `MASTER_HZ/CPU_DIV/PPU_DIV/SCREEN_W/H/PPU_DOTS/LINES` 和调试事件枚举；`open6502_clocks.sv` 里另有一份 `CPU_DIV/PPU_DIV` localparam 副本，理想情况应统一到 package。

规则：

- 不在 RTL 散落 magic number；至少 package 或模块 localparam。
- 位宽显式；比较/拼接不做隐式扩展。
- 枚举用 `typedef enum logic [...]`；状态机状态数留余量。
- 参数化只暴露真正需要变化的维度；不要为“看起来通用”加无用参数。

## 4. 总线契约

目标契约（Open6502 START 恢复计划提出）：

```systemverilog
output logic        bus_valid;
output logic        bus_we;
output logic [15:0] bus_addr;
output logic [7:0]  bus_wdata;

assign bus_strobe = cpu_ce && bus_valid;
```

规则：

- `bus_strobe` 是外设**唯一提交点**；每个 CPU 访问恰好一次。
- 禁止外设对“电平保持多个 master clock”的地址/写使能重复提交。
- 读/写副作用只在 strobe 时发生；dummy read 也必须真实发生（不能因标记省略）。
- 地址译码集中在一个 decoder；每个 region 的镜像/权限写清。
- 请求/响应要有容量与进展保证；不能接受请求后丢响应。
- 多主设备（CPU/DMA/外设）用独立 initiator + 仲裁；统计要能暴露饥饿。
- 读数据与写数据路径分开；不要用同一根线双向驱动。

`[事实]` 当前 `open6502_top` 用 `cpu_ce` + 地址译码近似提交；新芯片应把 `bus_strobe` 显式化并加断言“每次访问恰好一个 strobe”。

## 5. CPU/控制器状态机

`[事实]` Open6502 CPU 用显式状态枚举 `ST_RESET_LO/ST_FETCH/ST_ABS_LO/...`，每个状态驱动确定的 `bus_addr`。

规则：

- 每个周期都有明确状态与总线行为；不允许“整条指令执行完再累加周期”。
- 覆盖：取指、操作数、跨页、分支、dummy read、RMW 双写、栈、JMP 间接环回、BRK/IRQ/NMI/RESET。
- 中断按采样/边沿锁存规则实现；不要用“为追平 REF 的补丁”当通用语义。
- 等待/停顿来自真实握手与资源占用；固定 513/514 周期只能当期望值，不能当实现。
- retire 是架构观察点，不等于所有设备事件结束；边界对齐用明确 phase。

## 6. 存储与外设

- RAM/ROM/MMIO/PRG RAM 是不同对象；禁止别名到同一数组。
- 地址镜像用显式 decoder；镜像规则写进 spec。
- 大数组优先 SRAM/BRAM 友好写法：单口/简单双口、同步读、无复位循环。
- 小寄存器阵列可用 FF，但要在面积报告里标注。
- 外设寄存器：写副作用一次；读副作用一次；状态位清除规则写清。
- 设备之间不直接访问私有存储；通过端口/接口。

## 7. 综合子集与 `SYNTHESIS`

`[事实]` Open6502 在 PPU 用 `ifdef SYNTHESIS` 限制 sprite 扫描、跳过存储器初始化；top 在综合时把部分 `dbg_*` 置零。

规则：

- 可综合子集：无动态数组/class/DPI/initial 延迟/文件 IO/`$display`。
- `ifdef SYNTHESIS` 只包仿真专用内容；每个差异写进文档（面积/行为/覆盖）。
- 不用 `initial` 做功能复位；不用 `#delay` 做功能时序。
- 避免锁存器：组合 `always_comb` 全赋值；if/case 覆盖 default。
- 避免组合环；需要时显式打拍并写理由。
- 三态只在 top IO 使用；内部用 mux。
- 综合后必须回跑同一套仿真/门禁，不能“综合过 = 正确”。

## 8. 可观测性

- 每个外部接口都有 trace：视频/音频/存储/输入/调试。
- top 保留 `dbg_*` 端口；事件带 `kind/tick/addr/data/we/source`。
- 高频事件用 DPI/共享内存；FPGA 导出摘要/哈希。
- 关键状态（NMI 计数、palette 写序列、DMA 状态）要可观测，否则无法定位 L3。
- 调试逻辑不能改变功能时序；综合时可裁掉，但仿真必须保留。

## 9. Testbench 规范

`[事实]` am6502 平坦 bench：

```systemverilog
vvp flat_tb.vvp +IMAGE=app.hex +TRACE=rtl.trace \
    +MAX_INST=N +STOP_PC=0x3469
```

规则：

- 每个 bench 只测一个契约；名字 `<chip>_<feature>_tb.sv`。
- 加载镜像用 plusargs；不硬编码 `/tmp` 路径。
- 每条 retire 打一行标准格式（如 `PC A X Y SP P`），与 REF 格式一致。
- 失败用 `$fatal(1, ...)`；成功 `$finish`；超时算失败。
- bench 不改 reset 向量/段表；需要特殊入口时由镜像构建解决。
- 打印 PASS 行要包含关键状态，方便日志比对。
- 回归脚本 `set -eu`；解析失败、空输出、长度不等都算失败。

## 10. Make 目标

最小集合：

```make
lint:        # Verilator --lint-only -Wall
sim:         # Icarus/Verilator 跑全部 bench
difftest:    # 同一镜像 REF vs RTL
test:        # 镜像矩阵 × 后端
synth-%:     # 单模块 Yosys stat
clean:
```

规则：

- 一条命令一个程序；`make test` 覆盖矩阵。
- 失败非零退出；超时非零退出。
- 不添加永远成功的占位目标。
- `clean` 只删生成物，不碰源码/第三方。
- CI 调用的目标与本地一致；不要 CI 专用隐藏脚本。

## 11. 文档规范

必备：

- `README`：一句话定位、当前状态、能做什么/不能做什么、快速开始。
- `architecture`：框图、时钟/复位、地址图、top ports、模块职责。
- `difftest`：REF/DUT、事件、同步点、报告、预算。
- `verification`：L0–L7、必需套件、发布门禁。
- `checklist`：S0–S6/P0–P6 逐项状态。
- `ADR`：每个不可逆决策（profile、端口、legacy/新模型、检查点、后端）。
- `scorecard`：八维评分与证据。
- `CONTRIBUTING`：PR 规则、测试要求、许可证/来源。

规则：

- 声明必须带命令/产物/限制；不写“已完成”而没有证据。
- 计划与实现分开写；不要把目标写成现状。
- 状态数字会过期；写“最后验证日期”，以 CI/git log 为准。
- 英文提交信息；文档语言按团队约定，但术语/命令保持一致。

## 12. PR 规范

```text
[ ] 改动小而聚焦
[ ] 先加/更新测试
[ ] lint / sim / difftest 通过
[ ] golden 未变，或已评审并说明
[ ] 没删测试/没弱化断言
[ ] 英文 commit message
[ ] 证据（命令 + 输出 + 限制）
[ ] 无版权 ROM / proprietary PDK
```

大改动先开 issue/ADR；不要用 revert 掩盖问题，向前修。

## 13. 代码审查清单（RTL）

- 时钟/复位是否唯一、相位是否确定？
- 每个总线访问是否恰好一个提交点？
- 是否有组合环/锁存器/未初始化状态？
- 跨域是否同步？多比特是否握手？
- 存储是否 SRAM 友好？是否可综合？
- 调试逻辑是否影响功能？
- 是否有对应 testbench/断言？
- 综合后是否还能过同一套门禁？
- 面积/时序变化是否解释？
- 文档/ADR 是否更新？
