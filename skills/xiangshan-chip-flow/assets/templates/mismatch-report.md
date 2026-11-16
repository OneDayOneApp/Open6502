# 首个差异报告

```text
run_id:
date:
dut_version:
ref_version:
config:
image_sha256:
input_sha256:
seed:

sync_point:        # instruction / bus / device / frame / audio
tick:
cpu_cycle:
scanline:
dot:
field:
dut:
ref:

context_before:    # 最近 N 条事件
context_after:

last_bus_events:
last_device_events:
last_irq_events:

state_hash_dut:
state_hash_ref:

first_divergence_index:
replay:            # 从该点复现的命令
checkpoint:        # 可选：从哪个 snapshot 恢复
```

## 定位结论

- 根因（假设）：
- 证据：
- 最小修复：
- 回归测试：
- 影响范围：
- 是否更新 golden：否 / 是（原因）

## 复现命令

```sh
<command>
```
