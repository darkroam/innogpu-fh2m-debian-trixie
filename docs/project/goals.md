# 项目整体目标与工作路线

## 状态

本文件是项目的**整体目标与工作路线**的唯一权威入口：说明"我们为什么做、要达成什么、按什么顺序
做"。具体任务状态由 [todo.md](../planning/todo.md) 跟踪；当前运行状态以
[status.md](status.md) 为唯一摘要；两者都不在本文件重复维护。

## 整体目标

让 FH2M（Innosilicon Fantasy II-M）GPU 在 Debian Trixie 上**稳定可用、能力可知、可维护可演进、
持续优化**：

1. **稳定可用**：驱动、显示、硬件 GL、音频、视频开箱即用，具备可回退、可恢复的完整安全链；
2. **能力可知**：完整掌握硬件能力面（Vulkan / OpenCL / GL / 视频 / 功耗），全部有实测证据与文档；
3. **架构升级（重构架构、取消 patch、直接纳入代码）**：从"厂商黑盒 + 补丁叠加"（patch 文件 +
   wrapper 开关）重构为"**Git 管理的导入驱动源码树 + 清单管理二进制**"——取消 patch 文件模式，内核驱动源码
   直接纳入仓库以提交迭代；黑盒二进制（.o_shipped/.so/固件）由 manifest 清单管理、幂等提取。
   代码可迭代、可测试、可复现（设计见 [source-tree-migration.md](../planning/source-tree-migration.md)）；
4. **持续优化**：针对实测热点与正确性缺陷持续修复，并把每项成果沉淀为可追溯的源码、探针与文档。

## 子目标与状态

| 子目标 | 状态 | 证据 / 入口 |
| --- | --- | --- |
| 稳定运行基线 | 达成（4.0.0-i1 实机运行；patched-27 为已验证回退基线） | [status.md](status.md)、[Phase 4 验收](../planning/phase4-device-validation.md) |
| 能力面普查 | 部分达成（Vulkan/OpenCL 最小执行与 GL 已验证；VA-API H.264 Main + HEVC Main 实际解码、DMA-BUF 同设备 PRIME self-import 回归已验证；其余 profile/编码及跨设备路径待验） | [test-strategy.md](test-strategy.md)、[capability-survey.md](../planning/capability-survey.md) |
| 逆向可行性评估 | 达成（四层可行性 + 谱系判定） | [reverse-engineering-assessment.md](../planning/reverse-engineering-assessment.md) |
| DDK 谱系对照表 | 达成（组件 / UAPI / 特性 / 用户态映射） | [ddk-v119-mapping.md](../planning/ddk-v119-mapping.md) |
| 内核正确性修复 | 3/3 达成（dma_resv usage / vblank 守卫 / foreign DMA-BUF） | patch-025/026/027 |
| 构建可复现 | 达成（目录 mtime 归一化修复，三包逐字一致） | [release 审阅](../planning/release-review-2026-08-20.md) |
| 源码树迁移 | **阶段 0–4 完成**（设计冻结 ✅、drivers/ 导入 + 9 补丁转提交 + parity ✅、manifest + 幂等提取 + staging 内核编译 ✅、新构建器 4.0.0-i1 并行验证 ✅、实机候选验证 + p27 回退演练 ✅——设备已运行 4.0.0-i1）；阶段 5 第一步（标记 deprecated + 文档同步）完成，第二步（移入 legacy/）待条件满足 + 监督批准 | [source-tree-migration.md](../planning/source-tree-migration.md)、[phase5-retirement-design.md](../planning/phase5-retirement-design.md) |
| 性能优化（预取等） | 未开始 | [评估候选 4](../planning/reverse-engineering-assessment.md) |
| 能力深挖（codec 编码 / DVFS / CORE_ID） | 未开始 | [todo.md](../planning/todo.md) |
| 发布与跨硬件 | 阻断（许可证待核实；跨硬件/电源矩阵待做） | [source-license-audit.md](source-license-audit.md)、[suspended.md](../planning/suspended.md) |

## 工作路线

### 已完成阶段

1. **稳定化**（patched-17 → patched-24）：Debian 6.12 兼容、DPU/fbdev/connector/GEM 修复；
2. **能力普查 + 逆向评估**（2026-08-20）：能力面实测、可行性分层、DDK 谱系对照表；
3. **内核正确性三连修复**（patched-25/26/27）：fence 语义、vblank 守卫、DMA-BUF 生命周期；
4. **release 审阅**（2026-08-20）：构建可复现性修复、当前状态文档同步；
5. **源码树迁移阶段 0–4**（2026-08-21）：9 个启用补丁转源码提交，完成 manifest、新构建器、
   oracle 对比、4.0.0-i1 实机验收与 p27 回退演练；
6. **Phase 5 第一步**（2026-08-21）：旧构建器和 wrapper 标记 deprecated，当前入口切换为新构建器；
7. **runtime 测试第一阶段**（2026-08-24）：35 项能力基线落地，Vulkan queue 提交、OpenCL kernel
   读回、VA-API H.264/HEVC Main 实际解码与 DMA-BUF 同设备 PRIME self-import 回归在真机 PASS，当前汇总 22 PASS / 9 SKIP / 4 UNVERIFIED。

### 迁移阶段状态

源码树迁移由监督分支 `migration/supervised-source-tree` @ `bd76e91` 中的
`docs/planning/migration-supervision.md` 管辖；该监督指南只存在于监督分支，不是 `main` 的文档入口：

| 阶段 | 工作内容 | 当前状态 |
| --- | --- | --- |
| 0 设计冻结 | 目录、manifest、提取、staging、版本与回退策略 | ✅ 完成 |
| 1 源码树导入 | `drivers/` 导入，9 个启用补丁转换为源码提交 | ✅ 完成，parity PASS |
| 2 manifest 与 staging | 192 项 manifest、幂等提取、staging 构建 | ✅ 完成，G1-G7 PASS |
| 3 新构建器验证 | 新旧 oracle、模块符号、包边界与可复现性 | ✅ 完成 |
| 4 实机候选验证 | A1-A12、p27 回退演练、最终重装 | ✅ 完成，设备运行 4.0.0-i1 |
| 5 旧流程退役 | Step 1 标记 deprecated；Step 2 仅评估旧 wrapper 路径 | Step 1 ✅；Step 2 待发布周期、新设备验证和批准 |

详细设计见 [source-tree-migration.md](../planning/source-tree-migration.md)。

### 待办

1. **许可证发布阻断**：3 个 confidential 与 70 个无许可路径已**排除出公开制品**（`driver-source`
   非完整驱动，BLOCKED；`project-tools` 为候选制品，机械门禁 CLEARED，发布待监督批准）；
   **开放决策：GitHub 主分支是否作为发布目标**（当前仍公开分发阻断路径，仓库级发布未闭环）；
   关闭 `license_release_gate=BLOCKED` 前不得发布完整源码树或载荷附件，见
   [licensing.md](licensing.md)（唯一权威文档）；
2. **runtime 剩余能力**：modeset/热插拔/合盖、Picom backend、音频听感和跨硬件矩阵；
   （DMA-BUF 回归已于 2026-08-26 真机 PASS，不再列入待办）
3. **性能优化**：invisible READ 批量预取（候选 4，需先设计）；apphint 调优（候选 5）；
   上游 bugfix 移植（候选 6，依赖开源 DDK 可得性）；用户态调用画像（候选 8）；
4. **能力深挖**：DVFS/功耗实测与调参（候选 7）、CORE_ID/BVNC 直接读取、私有 codec 编码接口验证、
   WebKit DMA-BUF 上游修复报告；
5. **远期定向 RE**：`innogpu.o_shipped`（HAL）与 `innodma.o_shipped`（DMA）符号级分析，
   目标以"开源谱系 + 还原 HAL"组合替换预编译核心；
6. **发布收尾**：许可证阻断关闭后，完成跨硬件、电源/合盖矩阵及 release 附件审查。

## 计划任务完整清单

唯一任务清单为 [todo.md](../planning/todo.md)，按方向组织：

- 文档与维护（回退演练、脚本收敛、发布一致性）
- WebKit DMA-BUF 调查（应用级 workaround 已定，上游报告待整理）
- 逆向工程与能力挖掘（普查 ✅、内核修复 ✅、剩余运行时项、DDK 对照 ✅、预取、符号级分析）
- 源码树迁移（阶段 0–4 完成；阶段 5 Step 1 完成、Step 2 待条件满足）
- 发布收尾（跨硬件矩阵、release 附件）

## 文档导航

```text
README.md（入口）
  └─ docs/project/goals.md（本文件：为什么做、做什么、什么顺序）
       ├─ status.md（当前运行状态）
       ├─ architecture.md（代码架构与组件边界）
       └─ docs/planning/todo.md（具体任务清单）
            ├─ source-tree-migration.md（迁移设计）
            ├─ reverse-engineering-assessment.md（评估与候选）
            ├─ capability-survey.md（能力面）
            ├─ ddk-v119-mapping.md（谱系对照）
            └─ release-review-2026-08-20.md（发布审阅）
```
