# GEM6502 芯片架构与时序仿真平台编码说明

版本：1.0（目标设计）；日期：2026-11-11。

本文落实 [需求说明](requirements.md) 中 R01–R18。所有目录、API、配置和构建目标除明确标为“现有”外，均为待实施设计，不代表代码已实现。先实现 G0–G3 的最小闭环，再扩展 SoC；不得用游戏兼容性替代门禁。

## 1. 不可违反的实现约束

1. timing 模式下，全局时间只能由事件内核推进；组件不能直接修改全局 tick。
2. CPU 不能通过执行整条指令后累加周期来实现 timing；必须能在一次总线事务未完成时挂起。
3. timing 内存/MMIO/DMA 必须经过端口；不得直接访问其他组件的存储数组或调用其私有推进函数。
4. 数据副作用只能在协议规定的服务/采样事件发生一次，不能在请求入队时提前发生。
5. 资源等待来自队列、仲裁和握手，不能用事后补偿周期掩盖缺失的交互。
6. 每个可见状态转换必须有确定的时间、相位和因果来源。
7. SDL、声音设备和文件输出属于主机适配层；主机等待不能推进或改变模型时间。
8. 禁止静默从 timing 回退到功能执行，禁止为追平 NEMU 而跳过事务或复制参考状态。
9. 所有模型状态归具体实例所有；禁止可变的函数静态变量、全局设备单例和隐式共享时钟。
10. 芯片行为由版本化 profile 定义；NEMU 行为不是硬件规范。

## 2. 架构和目录边界

采用 C11、小范围公开头文件、每实例上下文和显式错误返回。对象化通过 C 接口表实现，不为模仿 gem5 外观而强制改用 C++。未来语言变更必须经 ADR。

建议目标布局：

```text
include/gem/        公共类型、配置、对象/端口接口、错误码
src/sim/           event、clock、object、reset、drain、checkpoint
src/mem/           packet、port、address_map、ram、rom、interconnect
src/cpu/           isa、functional_cpu、timing_cpu、microcode
src/dma/           oam_dma、dmc_dma、arbiter
src/devices/       ppu、apu、controller、mapper、中断连线
src/observe/       trace、statistics、manifest、diff adapter
src/platform/      芯片 profile、对象创建与端口连线
src/frontend/      CLI、SDL、输入录制/回放、音频输出适配
configs/           严格版本化的 JSON 配置和 schema
spec/              profile、接口契约、相位表、校准矩阵
tests/            kernel、ports、cpu_bus、dma、devices、checkpoint、diff
```

目录图中的 `tests/` 为项目根下测试目录，不在 `src` 中。阶段迁移不得一次搬动全部现有文件；新增子系统通过清晰适配层接入。

目标依赖方向：frontend → platform → CPU/DMA/devices → ports/interconnect → sim。sim 不依赖 NES、CPU 指令、SDL 或 ROM 格式；互连只认识地址范围、主设备和事务，不认识具体 opcode。

组件通过端口、连线和只读观测接口交流。PPU 对 CHR 的取数也必须可观测，不得绕过 mapper 端口，以免 MMC3 A12 活动再次变成扫描线近似回调。

## 3. 时间与事件内核（R01/R02/R17）

### 3.1 时间表示

`GemTick` 使用 `uint64_t`。一个 tick 的物理含义由配置冻结，默认以目标 master clock 周期为单位。CPU/PPU/APU 使用整数周期和显式相位；需要分数频率时，配置必须选择可精确表示的公共时间基准或经审查的有理数调度，不允许用浮点累加决定事件先后。

NES 基准可使用 master/12 的 CPU 和 master/4 的 PPU；具体 master 频率及初始相位写入 profile。不得在设备内部散落 `cpu_cycles * 3` 作为跨域调度机制。

### 3.2 调度键与同时间语义

调度键为 `(tick, delta, phase, sequence)`，全序比较。`sequence` 是实例内单调递增编号，用于稳定排序，必须进入检查点。

下表为内核默认逻辑阶段；硬件相位如何映射到这些阶段，由 profile 固定并校准，不能把表本身当作真实芯片波形：

| phase | 默认职责 |
| --- | --- |
| 10 | 时钟边沿、组件驱动、提出请求 |
| 20 | 请求队列与仲裁决定 |
| 30 | 地址/控制/数据驱动 |
| 40 | 规定采样点的读写与 MMIO 副作用 |
| 50 | 设备内部状态提交 |
| 60 | 响应交付与完成通知 |
| 70 | CPU 微操作状态更新 |
| 80 | profile 规定的中断采样 |
| 90 | 指令提交和只读观测 |

在相同 tick/delta，不能调度到已执行的 phase。需要返回较早 phase 的组合传播必须显式使用下一 delta；超过配置上限即报告零时间循环。跨硬件周期的动作必须增加 tick，不能靠 delta 隐藏周期。

入队时拒绝过去时间和溢出。回调执行前先更新当前时间及 phase。提供 schedule、cancel、reschedule、run_until 和稳定 event handle；取消事件不得留下可执行的悬空参数。`run_until(t)` 处理约定边界内全部事件后停在 t，不越界，不回退。

### 3.3 生命周期与状态

事件使用稳定的 `(owner_id, event_kind, payload)` 标识，运行期分发表可映射到函数；存档不保存函数指针。事件数据不得引用栈内临时对象。复位递增 generation 或取消相关事件，明确处理旧事务，防止复位后旧回调继续修改状态。

CPU 等待期间，其时钟和其他设备仍能运行。可安全跳过无状态变化的区间，但必须证明下一个可见事件、状态和统计完全等价；优化前后规范化 trace 必须一致。

## 4. 组件、配置与连接（R03/R05/R11/R17）

对象接口必须提供：稳定 ID、路径名、类型/版本、参数、端口、init、bind、validate、startup、reset、drain、serialize、restore、statistics。创建、连线、校验完成后才允许调度事件。

配置采用严格 JSON：`schema_version`、profile、时间基准、时钟域、CPU 模型、对象、地址图、互连、仲裁、队列容量、trace、输入及种子。未知字段、重叠地址范围、断开的必需端口、非法分频、无效等待协议均使启动失败，不允许忽略字段继续运行。

实际使用的完整配置输出到 manifest，包含默认值展开结果。不得把配置到处转换成全局变量。运行中可修改的参数必须声明生效边界并产生配置事件；其余参数仅可在 drain 后或重启时修改。

所有内存由对应 memory/device 对象持有。只读观测快照与设备状态分离，禁用调试器或前端通过 `g->ppu.*` 等字段直接改变执行中的 timing 模型。

## 5. 事务和端口协议（R04/R05）

以下为目标接口轮廓，实施时必须提供完整错误码、类型和状态机文档：

```c
typedef uint64_t GemTick;
typedef uint64_t GemTxnId;
typedef uint32_t GemObjectId;

typedef enum { GEM_READ, GEM_WRITE } GemCommand;
typedef enum { GEM_ACCEPTED, GEM_RETRY, GEM_ERROR } GemSendResult;

typedef struct {
    GemTxnId id;
    GemObjectId initiator;
    GemCommand command;
    uint32_t address;
    uint32_t size;
    uint8_t data;
    uint64_t parent_event;
    uint64_t instruction_id;
    bool dummy_access;
} GemRequest;

GemSendResult gem_port_send_timing(GemPort *, const GemRequest *);
void gem_port_notify_request_retry(GemPort *);
bool gem_port_receive_response(GemPort *, const GemResponse *);
void gem_port_notify_response_retry(GemPort *);
```

6502 首期事务宽度为一字节，`size != 1` 必须拒绝；上层结构预留扩展不代表已经支持突发或多字节原子性。所有请求携带真实发起方、关联 ID 和 dummy 标记；dummy access 必须发生真实的设备副作用，不能因标记而被省略。

### 5.1 所有权与进展保证

- `ACCEPTED`：接收方复制请求并取得该事务的执行责任；发起方保留 ID/等待状态，不再发送第二份。恰好交付一次响应或显式失败。
- `RETRY`：没有产生任何副作用，没有转移事务；发起方保留原请求，等待通知。禁止忙轮询和零 tick 重试循环。
- 请求重试通知只代表允许重发，不保证接受；通知注册必须是电平安全的，不能错过队列腾空导致永久等待。
- 响应不被接受时，响应方保留同一响应并等待响应重试通知，不得重新执行读写副作用。
- 请求、响应两向都必须有容量策略和进展保证。禁止接受请求后因没有响应缓存而丢弃完成结果。
- reset/drain/设备错误必须显式完成或终止相关事务；不能让等待 CPU 永久悬挂。

### 5.2 延迟与副作用

必须区分发起、接受、仲裁获胜、服务开始、采样/写入、响应就绪、响应交付。延迟的单位和是否占用总线由组件契约定义。

MMIO 在 profile 规定的采样事件读取或写入一次。写请求进入等待队列不等于写入生效。响应背压不重复清除状态位、推进手柄移位或写入存储器。

服务时地址映射还是发起时锁定映射，必须在 profile 中明确，特别是 mapper bank 写入与未完成访问的关系；默认禁止未经说明的 bank 状态缓存。

### 5.3 功能与原子访问

`functional_peek` 不推进 tick、不修改设备/统计，不伪造 MMIO 读副作用。加载/调试写仅在声明的静止/drain 边界进行，并验证没有未完成事务；需要在线调试写时，必须另定义对在途事务和副本的一致性规则。

atomic 访问同步完成并返回估算延迟，不能用于混入正在运行的 timing 内存系统。不能简单把当前 `side_effect=false` 当作完整 functional 接口，更不能把带副作用的同步访问称为 timing。

## 6. CPU 微操作执行（R06/R08）

保留现有 `cpu_step()` 作为命名明确的 legacy/functional 模型。新 timing CPU 必须有独立执行状态：opcode、微操作索引、指令起始 PC、临时操作数、有效地址、待完成事务、采样状态、提交记录。

基本路径为：时钟边沿 → 选择微操作 → 请求取指/读/写 → 等待接受与完成 → 指定边沿采样 → 更新内部状态 → 下一微操作/提交。没有总线访问的内部周期，也必须有明确的状态和边沿。不能在响应到达后把未来微操作全部在同一 tick 执行完。

官方指令及配置声明的非法指令，必须有可审查的逐周期操作表。表可复用 ISA 解码和纯运算函数，但不能复用同步读写形成隐藏捷径。跨页、分支、栈、JMP 间接环回、RMW double write、dummy read、BRK/IRQ/NMI/RESET 均要覆盖。

引脚和等待规则必须区分 profile。不能假定 NMOS 6502 的 RDY 对任意读写都能任意等待；某种时序实验需要可拉长写周期时，必须在该 SoC profile 中定义相应接口。等待时间测试先使用人工可计算的读访问序列。

指令 retire 是架构观察点，不等于所有设备事件结束。边界对齐必须使用明确 phase，不靠比较脚本猜测“相差一条记录”。中断按硬件采样/边沿锁存规则实现，不能把上一次 NEMU 兼容性补丁直接写成通用语义。

## 7. 互连、存储器与 DMA（R05/R07/R12）

互连负责解码、队列、仲裁和服务时序；RAM/ROM/MMIO 负责存储或设备语义。地址镜像必须由显式 decoder 完成。PRG RAM 与 CPU RAM 是不同对象，不得别名到同一数组。

仲裁策略为独立可配置对象，支持 profile 固定优先级和实验用策略。实验策略必须带 profile 标签，不能冒称 NES 硬件策略。公平策略必须给出有限请求下的进展界限；有意使用可能饥饿的固定优先级时，统计必须能暴露饥饿。

CPU、OAM DMA、DMC DMA 为独立 initiator。DMA 使用状态机表示对齐/暂停、源地址读、目标写、剩余计数、请求等待和结束。OAM DMA 禁止使用一次循环复制后补停顿；DMC 不得通过 `functional_peek` 直接抓取样本绕过仲裁。

复位、DMA 期间的中断、OAM 地址环回、读写半周期、DMC 与 OAM 重叠均有独立短波形测试。513/514 周期来自正确协议和起始相位的运行结果，不是实现停顿的输入常量。

统计需记录请求/响应守恒、每个主设备的等待与服务时间，以及 idle/busy 的互斥资源占用。测量窗口边界的在途事务单独计数，不能错误要求所有窗口都满足请求数等于响应数。

## 8. PPU、APU 与中断连线（R08/R09/R10）

PPU 按自身时钟调度 fetch、滚动更新、sprite evaluation、可见像素及寄存器行为。CHR/nametable 访问必须记录真实地址活动，MMC3 A12 边沿与低电平过滤由这些事件驱动。禁止在 timing 模式使用“每条扫描线调用一次 MMC3 IRQ”代替。

现有逐像素 rasterizer 可以暂作输出适配器；它不是已校准的 fetch/shift 或 sprite evaluation 模型。开发阶段保留时必须标为 approximate，正式 G4/G6 不能依靠这个标记豁免缺失的时序。

APU 只有一个 frame sequencer 时间源，统一驱动包络、长度、sweep、linear counter 和 IRQ，禁止保留当前 PCM 合成与 CPU IRQ 两套独立时序计数。音频采样为有确定相位的模拟域事件，SDL 仅消费已生成的 PCM。

IRQ/NMI 经信号端口或独立中断控制连线进入 CPU，带来源和变化 tick。电平合并不能丢失仍有效的其他来源；NMI 边沿锁存、IRQ 确认和屏蔽的行为按 profile 分别验证。

## 9. 检查点、drain 与复位（R13/R17）

drain 停止发起新事务，允许在途协议完成；CPU、总线和 DMA 达到声明的安全边界后，所有组件统一冻结在确定的仿真时间。周期设备不能因为持续调度时钟而令 drain 永不结束；其暂停和恢复契约需明确。

第一版可只支持 drained checkpoint，但必须保存微架构、设备、时钟相位、全局 sequence、将来的设备事件、RAM/ROM 引用摘要、输入回放位置和统计窗口。不能把“没有在途总线请求”等同于“没有未来事件”。任意 in-flight checkpoint 是后续能力，只有显式保存队列、等待方及事务状态后才能启用。

使用版本化字段序列化和端序约定，禁止对整个 C 结构体直接 `fwrite`。指针、padding、回调地址、SDL 设备号及 FILE 不可序列化。对象 ID 和 event kind 在恢复时重新绑定，所有输入均验证长度、范围和配置摘要。

复位分冷启动/软复位，并逐 profile 列出保留状态。清除或重新绑定事件、事务、IRQ 来源和输入状态。当前 `gem_cart_a12()` 内可变静态状态等实例外状态必须移除。

## 10. Trace、统计、DiffTest（R12/R14/R15/R18）

事件 schema 必须版本化，整数 tick 不使用浮点 JSON number 造成精度丢失；可用字符串或明确的二进制编码。记录字段遵循需求说明。支持可控 trace 类别、首差异停止和环形历史缓冲，避免必须保存数 GB 日志才能定位问题。

DiffTest 分开实现以下适配器：

- 功能 CPU ↔ NEMU：对齐架构提交点，验证指令语义。
- timing CPU ↔ 独立总线序列期望：验证 dummy/RMW/等待/中断等微架构行为。
- timing SoC ↔ RTL：对齐 master tick、相位、复位、总线事件和设备状态。
- 端到端负载：统一输入回放、状态和帧/音频检查，验证系统回归。

NEMU 不精确或两边共享实现的部分不能作为独立时序校准证据。实际硬件数据与 NEMU 冲突时，记录版本化证据并审查 profile，不能篡改 trace、跳过差异或强行同步状态。

比较器必须拒绝空输出、格式错误、异常退出、截断和长度不等；状态位屏蔽必须逐字段有硬件依据。同步阶段差异在 adapter 明确转换，不做任意周期偏移。MMIO 总线读取本身不得在“采样状态”时再次执行。

首差异报告包含双方配置/版本、同步点、tick/相位、字段、双方值、最近 CPU/总线/DMA/IRQ/PPU 事件、回放输入及恢复命令。校准报告逐组件记录时间偏差和容许条件，不用一个“全部通过”隐藏未覆盖区域。

## 11. 实施顺序、构建目标与追踪矩阵

下列 `make` 目标为待实现接口。每个阶段提交必须包含实现、必要测试、配置、结果及剩余限制；不能添加永远返回成功的占位目标。

| 阶段 | 目标文件/能力 | 目标验收命令 | 需求/门禁 |
| --- | --- | --- | --- |
| A | profile、相位表、地址图、端口契约 | `make spec-check` | R03/R05/R08/R11/R15/R18；G0 |
| B | event/clock/object/reset | `make test-kernel` | R01/R02/R03/R17；G1；T02/T09 |
| C | ports、RAM、bus、简单流量发生器 | `make test-timing-bus` | R04/R05/R07/R12；G2；T01/T03 |
| D | 少量 timing 指令形成最小闭环，再补全指令 | `make test-cpu-timing` | R06/R08；G3；T01/T04/T05 |
| E | OAM/DMC、PPU/APU/mapper 时序接入 | `make test-soc-timing` | R07–R10；G4；T03/T04/T10 |
| F | drain/checkpoint/config/统计/RTL adapter | `make test-checkpoint test-rtl-diff` | R11–R18；G5；T06–T09 |
| G | 独立测试、完整回归及校准报告 | `make validate-chip` | R14/R15/R18；G6 |

最小闭环必须真实运行：timing CPU 发请求 → 互连接受/排队 → RAM 延迟响应 → CPU 恢复执行；此时定时设备仍继续运行。先支持 LDA/STA/NOP/分支等短程序即可，但必须显式标为未完成的实验模型。

现有 `make test/difftest/games/mmc3/test-ui` 是 legacy 回归。新增 timing 测试不得复用这些目标的名称暗示已经通过芯片级门禁。CI 输出必须区分 legacy、experimental timing、calibrated timing。

## 12. 迁移、代码审查和发布纪律

- 先固定当前 legacy 基线，再建立独立 timing 路径；禁止将现有行为静默改名为 timing。
- 可复用 ISA 运算、加载器、测试 ROM 和前端。同步 CPU 执行路径、直接内存访问、固定 DMA 停顿及批量设备推进必须逐步退出 timing 路径。
- 迁移完成前，原命令保持明确 legacy 标签；新接口使用显式 `--mode=timing --config=...`，未经 G6 不默认宣称芯片级精度。
- 每项 PR 标明 R/G/T 编号、支持的 profile、仿真可见行为变化、独立证据和未实现部分。硬件语义变更须附规范/波形及边界测试。
- 核心库独立于 SDL 构建；使用严格告警、ASan/UBSan 和确定性测试。检查点解析和配置解析覆盖错误输入；协议状态机覆盖拒绝、背压、复位及完成。
- 优化必须保留规范化事件序列或有审查过的等价证明；降低精度必须成为显式模型配置，而不是隐藏的快速路径。
- 复用 NEMU/gem5/第三方代码时保留版权、许可证及来源，并在分发前审核兼容性；共享实现的验证结果标为非独立证据。
- 游戏窗口、声音或帧数增加不计入 G1–G5 完成度。发布清单按门禁逐项提供命令、产物和已知限制。

## 13. 设计依据与后续 ADR

事件内核、端口语义与检查点的能力参考需求说明中的 gem5 官方资料；本文件的 phase 编号、C 接口、目录与 gate 是 GEM6502 的设计决策，不是照抄 gem5 的硬件语义或兼容性承诺。

编码开始前必须建立 ADR-001（profile 与相位/采样规则）、ADR-002（端口/事务所有权与进展）、ADR-003（legacy 与 timing CPU 共存）、ADR-004（检查点安全边界）、ADR-005（RTL 对齐与独立校准）。涉及未冻结硬件语义时先给出可审查规范，不能用实现偶然行为反推规格。
