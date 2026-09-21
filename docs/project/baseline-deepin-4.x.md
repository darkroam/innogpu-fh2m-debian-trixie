# 代二：deepin 4.0.x 源码树迁移线

本代把 Deepin 202504 的可维护源码迁入 `drivers/`，以 manifest 约束黑盒载荷，
最终冻结为 `4.0.2-i3`。它是当前 fantgpu 诊断线的回退基线，历史 Deepin 验收不能
外推为 fantgpu 5.0.0-iN 已通过。

本页为 R17 第 2 轮草稿，待 qoder 初审、dsh 终审。代际事实基准为本机
`collab/R17-2026-09-19-文档优化迭代/report.md#L359-L380 @ 3e356aba9b5d`（§21）。
当前状态见 [status](status.md)，前代见 [legacy patched 阶段](baseline-legacy-patched.md)。

## 身份与事实

| 项目 | 已核对事实 | 来源 |
| --- | --- | --- |
| 起止事件 | 2026-08-21 源码树迁移至 2026-09-09 `deepin-4.0.2-i3` 冻结 | §21；[迁移设计](../planning/source-tree-migration.md)；tag 说明 |
| tag 与目标提交 | `deepin-4.0.2-i3` → `b30c8071e595` | annotated tag；不可移动 |
| 目标提交日期（+08:00） | 2026-09-09 12:41:23 | Git commit |
| tag 创建日期（+08:00） | 2026-09-09 12:48:17 | Git taggerdate；与目标提交日期分列 |
| 载体 | Deepin 202504 源码树、manifest 黑盒载荷及迁移补丁；终点组合为 024/026-lifecycle/028/029 | [provenance](../planning/patch-provenance.md)、[patch-029](../patches/029-suspend-resume-ddcci-panel.md) |
| ABI 边界 | 用户态、固件与 shipped objects 保持 Deepin 同源；源码迁移不等于闭源核心源码化 | [迁移设计](../planning/source-tree-migration.md) |
| 代表 deb / 回滚卡 | `build/innogpu-fh2m-trixie_4.0.2-i3.deb`，本轮实物 SHA 与记录一致 | tag 说明；[维护策略](maintenance-policy.md) |
| 再下一层回退 | `4.0.0-i1`，随后为 patched-27；具体操作以 [recovery](../user/recovery.md) 为准 | 不执行本页引用的历史命令 |

代表 deb 的完整 SHA-256：

```text
177133eebda692092501a27d7d135662ddaedaf3634776b8aa1ea5153c9e1662
```

## 本代工作

把可维护源码与黑盒载荷分开：Git 保存 `drivers/` 源码和来源清单，`vendor/`、`build/`
保存本地载荷和产物。迁移将 9 个启用补丁转为源码提交，保留 patch provenance；
000 继续是确定性二进制变换工具。p27 oracle 对比、模块符号检查和固定 epoch 双构建
约束迁移等价性，历史补丁与回退包继续保留，详见
[源码树迁移](../planning/source-tree-migration.md) 和 [Phase 4 验证](../planning/phase4-device-validation.md)。

后续 suspend/resume 工作形成 024、026-lifecycle、028、029 组合。失败过程包括
PowerLock 时序竞态、s2idle 红屏及独立温度 work 过早启动；不能用最终结果抹掉中间失败。
因果链见 [deep 事故](../incidents/suspend-resume-deep-reproduction-20260902.md)、
[s2idle 红屏事故](../incidents/suspend-resume-s2idle-red-screen-20260902.md) 与
[patch-029 的最终矩阵](../patches/029-suspend-resume-ddcci-panel.md)。

`4.0.2-i3` 的 R14 当前设备矩阵为 6/6 deep，R16 阶段二另有真机矩阵与安装回退证据。
`deepin-4.0.2-i3` tag 标记 Deepin 血缘终止点及 F0 迁移起点，不能将这一历史签发
改写为当前 fantgpu 的签发，也不能把仍为 UNVERIFIED 的能力补成 PASS。

## 与上一代差异

| 维度 | legacy patched 阶段 | deepin 4.0.x 源码树迁移线 |
| --- | --- | --- |
| 组织方式 | 解包原 deb 后叠加补丁 | 可维护源码直接入树，黑盒经 manifest 就位，再隔离构建 |
| 载荷关系 | p27 已使用 Deepin 202504 | 延续该载荷血缘；改变组织方式，不混入 fantgpu 的闭源 ABI |
| 追溯方式 | wrapper 开关、patch、版本 deb 与 tag | 增加源码转换提交、manifest、oracle parity 与分阶段验证 |
| 版本角色 | p27 历史回退包 | i3 为 Deepin 冻结终点和当前回滚卡；失败候选不因同属 4.0.x 而获准使用 |

下一代 [fantgpu 5.0.0-iN](baseline-fantgpu-5.x.md) 更换为 F0 源与 030 重推链。
迁移设计中“取消 patch”的历史表述不废止后来确立的
[双轨变更纪律](multiagent-collab.md#十一双轨变更纪律两条腿)。

## 溯源与验证边界

- `docs/planning/source-tree-migration.md#L1-L37 @ b7cdeb4aa8c1`：阶段状态、源码与黑盒边界；其中“当前”是迁移当时状态。
- `docs/planning/patch-provenance.md#L3-L24 @ b7cdeb4aa8c1`：9 个转源码补丁及分类。
- `docs/patches/029-suspend-resume-ddcci-panel.md#L30-L57 @ b7cdeb4aa8c1`：组合、矩阵与验证边界。
- `deepin-4.0.2-i3` tag 对象：目标 `b30c8071e595`、包 SHA、双构建及阶段二裁定；日期由 Git 对象只读取得。

DDCCI 仍不提供亮度控制或 backlight device，`hwinfo_g0m.bin` 仍缺失，display 025
为 UNVERIFIED 且未包含；本机矩阵不证明跨设备或长期压力。当前冻结继续适用：
`OUTSIDE_COVERAGE`、`R5=FAIL`、禁止重跑、U1/U2 未执行、validation-results 未签、
签发冻结、fantgpu 5.0.0-iN 未打 tag。回退须遵循既有回滚卡和独立授权。
