# PR 检查单

```text
[ ] 改动小而聚焦，一个 PR 只做一件事
[ ] 先加/更新测试（新增行为有回归，修复有最小失败用例）
[ ] lint 通过
[ ] sim / 单元 / SoC 冒烟通过
[ ] difftest 通过（相关范围）
[ ] 确定性双跑一致（如涉及时间/调度）
[ ] golden 未变；或已单独评审 + 原因/影响/重跑证据
[ ] 没有删除/弱化测试；known-fail 有 issue 和期限
[ ] 没有把“能跑”当“正确”的声明
[ ] 综合后回跑同一套门禁（如涉及可综合逻辑）
[ ] 文档/ADR/checklist 已更新
[ ] 英文 commit message
[ ] 无版权 ROM / proprietary PDK / 未授权 IP
[ ] 证据：命令 + 输出 + 已知限制
```

## 证据块（贴到 PR 描述）

```text
## What changed
<一句话>

## Why
<问题/需求/ADR>

## Evidence
- `make lint`：PASS
- `make sim`：PASS，<N> tests
- `make difftest`：PASS，<N> instructions
- 双跑：<hash1> == <hash2>
- 综合：<module> <cells>（口径：generic cells）
- 波形/日志：<path>

## Limits
- <未覆盖/近似/待办>

## Golden
- [ ] unchanged
- [ ] changed（原因：；旧：；新：；评审：）
```

##  reviewer 关注点

1. 这个改动有没有让某个门禁变弱？
2. 失败时能不能定位首个差异？
3. 有没有不可观测/不可综合/不可复现的写法？
4. 声明的证据能不能自己跑一遍？
5. 有没有隐藏的范围蔓延？
