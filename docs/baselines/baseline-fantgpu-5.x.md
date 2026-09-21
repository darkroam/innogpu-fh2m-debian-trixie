# 代三：fantgpu 5.0.0-iN（当前诊断线）

本代以 fantgpu `3.3.8.126` F0 源重新推导 030-NNN 链，当前为 `5.0.0-i6` 诊断线。
构建可复现和首启健康均不等于发布验收；`R5=FAIL`，根因仍未定位。

本目录 docs/baselines/ 与仓库根 baselines/（运行结果归档）同词异物、互不相关。
本页为 R17 第 2 轮草稿，待 qoder 初审、dsh 终审。代际事实基准为本机
`collab/R17-2026-09-19-文档优化迭代/report.md#L359-L380 @ b5816b7c5a41`（§21）。
当前结论见 [status](../project/status.md)，前代见 [deepin 4.0.x](baseline-deepin-4.x.md)。

## 身份与事实

| 项目 | 已核对事实 | 来源 |
| --- | --- | --- |
| 起止事件 | 按 §21 的文档代际口径，自 2026-09-10 起，进行中 | 不把此起点等同于更早的迁移分支创建时间 |
| 代表实现提交 | `26dded58857a`，030-034 与 i6 诊断构建 | Git 对象存在；仅代表 i6 实现点，不是最后文档提交 |
| 目标提交日期（+08:00） | 2026-09-16 17:43:39 | Git commit |
| tag / tag 创建日期 | fantgpu 5.0.0-iN 未打 tag / 不适用 | meta 中 `tag` 是预留名称，不证明 tag 存在 |
| 载体 | F0 + 030 链物化为 O_stage，i6 meta 实列 18 项 | [i6 meta](../planning/evidence/o-stage/5.0.0-i6/5.0.0-i6.meta.json) |
| ABI 边界 | F 同源用户态、固件与 shipped objects；不得复用 Deepin 私有 ABI 假设 | [O_stage 方案](../design/o-stage-integration-plan.md)、[i4 ABI 事故](../incidents/r5-i4-oops-dev-rsrc-abi.md) |
| 代表 deb | `fantgpu-fh2m-trixie_5.0.0-i6.deb`，A/B 本地实物 SHA 均与已提交记录一致 | [构建摘要](../planning/evidence/o-stage/build-5.0.0-i6.sha256)；下述归档路径 |
| 回滚物 | `build/innogpu-fh2m-trixie_4.0.2-i3.deb`，SHA-256 前缀 `177133eebda6` | [代二完整身份](baseline-deepin-4.x.md)、[恢复规程](../user/recovery.md) |

代表 deb 的完整 SHA-256：

```text
0d7a269d74794e81ddf4412b5aff3f5581752773fcfc7b7843b4407b54d8ad49
```

本轮分别复算的本地实物（gitignored，不随 Git 分发）：

- `.runtime-archive/runtime-5.0.0-i6/build/A/fantgpu-fh2m-trixie_5.0.0-i6.deb @ 0d7a269d7479`
- `.runtime-archive/runtime-5.0.0-i6/build/B/fantgpu-fh2m-trixie_5.0.0-i6.deb @ 0d7a269d7479`

## 本代工作

先建立 F0、Deepin 基线与编排树身份，再逐项重新裁决补丁语义，形成可按顺序重放的
030 patch/meta 链。版本物化保留独立快照、manifest 和构建证据，不覆盖失败前代。
源码语义采用“直接修改 + patch 记录”双轨，见 [030 映射表](../planning/030-mapping-table.md)
和 [双轨规约](../project/multiagent-collab.md#十一双轨变更纪律两条腿)。

链的编号不是数量：i6 是 18 个显式条目，链尾为 030-034，含独立的 030-026-lifecycle。
i4 因探针状态改变共享 `dev_rsrc` 布局而首启 Oops；030-033/i5 改用独立 devres 并增加
源码与编译级 ABI 门。i6 增加 stop-stage 调查入口，双构建字节一致；这条修正与
构建证据不解除 R5，详见 [ABI 事故](../incidents/r5-i4-oops-dev-rsrc-abi.md)。

R5 调查经历 marker、正常态探针、stop-stage、watchdog 和两轮诊断内核。
`6.12.101-r5dpm1`/`6.12.101-r5dpm2` 是诊断内核版本，不是新的驱动代际。
两轮复核均为 `OUTSIDE_COVERAGE`，DPM 机制至今无动态正样；未覆盖区与机制不可触发
不可分辨，noirq 不作运行判断，不能把静默当成已定位或已排除全部回调挂死。
脚本缺陷与设计期越界过程分别见 [闸门缺陷重蹈](../incidents/script-gate-defect-recurrence.md)
和 [越界产物](../incidents/design-phase-out-of-bounds-patch.md)。

## 与上一代差异

| 维度 | deepin 4.0.x 源码树迁移线 | fantgpu 5.0.0-iN |
| --- | --- | --- |
| 基座与载荷 | Deepin 202504；i3 冻结点 | fantgpu 3.3.8.126 F0 与同源 F 载荷 |
| 修改追溯 | 源码迁移提交、历史 patch provenance | 030 逐项重推、meta 与树 hash 链、独立版本物化证据 |
| 验收角色 | 当前设备的历史交付及回滚卡 | 当前诊断线；未签发，不能继承 Deepin 的 deep PASS |
| 风险边界 | 历史范围内已验证的 Deepin 组合 | R5 未定位；新增 ABI 防线与构建证明只覆盖各自检查对象 |

## 溯源与验证边界

- `docs/planning/evidence/o-stage/5.0.0-i6/5.0.0-i6.meta.json#L1-L151 @ b7cdeb4aa8c1`：版本、F0、18 项链与未验证项。
- `docs/planning/evidence/o-stage/build-5.0.0-i6.sha256#L1-L3 @ b7cdeb4aa8c1`：A/B 构建记录。
- `docs/planning/evidence/o-stage/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-step8-result.txt#L56-L66 @ b7cdeb4aa8c1`：复核判定及未定位边界；同文件 `#L111-L118` 为冻结与 dsh 边界。
- `docs/incidents/r5-i4-oops-dev-rsrc-abi.md#L1-L53 @ b7cdeb4aa8c1`：ABI 因果链及证据限制。

冻结：`OUTSIDE_COVERAGE`、`R5=FAIL`、禁止重跑、U1/U2 未执行、validation-results
未签、签发冻结、未打 tag。`postinst_current_kernel_only=release_blocker` 和许可发布
阻断不变。GRUB 恢复及诊断包保留状态由 [步骤 8 结果](../planning/evidence/o-stage/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-step8-result.txt)
记录；本页及其历史引用均不授权构建、安装、重启、测试或回退操作。
