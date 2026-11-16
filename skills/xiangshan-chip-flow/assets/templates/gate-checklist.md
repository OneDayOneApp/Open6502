# <CHIP> 门禁清单（S0–S6）

> 复制到 `docs/spec/checklist.md`。`[ ]` 未开始，`[~]` 部分，`[x]` 完成。
> 每项必须有命令 + 产物 + 已知限制；没有证据不算完成。

## S0 契约冻结

- [ ] chip brief（目标/范围/非目标）
- [ ] profile（时钟/复位/电气可见行为）
- [ ] 地址图（region/权限/镜像/副作用）
- [ ] top ports（含调试观测）
- [ ] 镜像/ABI 契约
- [ ] REF/DUT 边界与独立性
- [ ] ADR-001 profile
- [ ] ADR-002 端口/事务所有权
- [ ] ADR-003 legacy/新模型共存（如适用）
- [ ] 接口无 TBD

## S1 参考模型 + 镜像

- [ ] REF 可构建/版本化
- [ ] REF 通过 <suite>
- [ ] 镜像构建脚本
- [ ] 双构建 SHA-256 相同
- [ ] host 端口语义
- [ ] 每程序成功判据
- [ ] 超时判失败
- [ ] 调试产物（map/lst/sym）

## S2 单元 + 总线

- [ ] L0 单元测试
- [ ] L1 定向测试
- [ ] `bus_strobe` 单点提交
- [ ] 地址 decoder/镜像
- [ ] SoC 冒烟
- [ ] lint 无 error
- [ ] `make sim` 全绿

## S3 锁步 DiffTest

- [ ] 同一镜像/输入/初始状态
- [ ] retire 级比对
- [ ] 首个差异报告
- [ ] 回放命令
- [ ] ≥ <N> 指令/周期零差异
- [ ] 预算 ≥10^6 cycles/s
- [ ] 故意注入错误能红

## S4 CI + 确定性 + 检查点

- [ ] required checks
- [ ] 双跑一致
- [ ] trace 开关不改变状态
- [ ] drained checkpoint
- [ ] restore == 连续运行
- [ ] 失败上传 trace/waveform/manifest
- [ ] PR 红不能合

## S5 FPGA

- [ ] 全 top 综合
- [ ] BRAM 推断
- [ ] 时序约束/Fmax
- [ ] PLL/复位
- [ ] 视频/音频/输入
- [ ] ROM/Flash/SD 装载
- [ ] 板级 bring-up
- [ ] ≥1 小时稳定

## S6 ASIC/PDK

- [ ] SRAM 宏
- [ ] DFT/scan + 覆盖率
- [ ] clock/reset/power intent
- [ ] 标准单元综合
- [ ] STA 全 corner
- [ ] PnR
- [ ] DRC/LVS/ERC
- [ ] pad/ESD/package
- [ ] tapeout checklist 评审
- [ ] shuttle/资金

## 发布说明

| 版本 | 日期 | 已过门 | 已知限制 | 证据链接 |
|---|---|---|---|---|
| | | | | |
