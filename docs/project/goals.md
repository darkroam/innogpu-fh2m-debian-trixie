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
   wrapper 开关）重构为"**自有驱动源码树 + 清单管理二进制**"——取消 patch 文件模式，内核驱动源码
   直接纳入仓库以提交迭代；黑盒二进制（.o_shipped/.so/固件）由 manifest 清单管理、幂等提取。
   代码可迭代、可测试、可复现（设计见 [source-tree-migration.md](../planning/source-tree-migration.md)）；
4. **持续优化**：针对实测热点与正确性缺陷持续修复，并把每项成果沉淀为可追溯的源码、探针与文档。

## 子目标与状态

| 子目标 | 状态 | 证据 / 入口 |
| --- | --- | --- |
| 稳定运行基线 | 达成（patched-27 实机运行） | [status.md](status.md)、[release 审阅](../planning/release-review-2026-08-20.md) |
| 能力面普查 | 达成（Vulkan 1.3.264 / OpenCL 3.0 / GL 4.3 / VA-API H264+HEVC 硬解等） | [capability-survey.md](../planning/capability-survey.md) |
| 逆向可行性评估 | 达成（四层可行性 + 谱系判定） | [reverse-engineering-assessment.md](../planning/reverse-engineering-assessment.md) |
| DDK 谱系对照表 | 达成（组件 / UAPI / 特性 / 用户态映射） | [ddk-v119-mapping.md](../planning/ddk-v119-mapping.md) |
| 内核正确性修复 | 3/3 达成（dma_resv usage / vblank 守卫 / foreign DMA-BUF） | patch-025/026/027 |
| 构建可复现 | 达成（目录 mtime 归一化修复，三包逐字一致） | [release 审阅](../planning/release-review-2026-08-20.md) |
| 源码树迁移 | 计划中（阶段 0 设计已落档，阶段 1–3 待执行） | [source-tree-migration.md](../planning/source-tree-migration.md) |
| 性能优化（预取等） | 未开始 | [评估候选 4](../planning/reverse-engineering-assessment.md) |
| 能力深挖（codec 编码 / DVFS / CORE_ID） | 未开始 | [todo.md](../planning/todo.md) |
| 发布与跨硬件 | 部分（审阅完成；跨硬件/电源矩阵待做） | [suspended.md](../planning/suspended.md) |

## 工作路线

### 已完成阶段

1. **稳定化**（patched-17 → patched-24）：Debian 6.12 兼容、DPU/fbdev/connector/GEM 修复；
2. **能力普查 + 逆向评估**（2026-08-20）：能力面实测、可行性分层、DDK 谱系对照表；
3. **内核正确性三连修复**（patched-25/26/27）：fence 语义、vblank 守卫、DMA-BUF 生命周期；
4. **release 审阅**（2026-08-20）：构建可复现性修复、当前状态文档同步。

### 进行中

5. **源码树迁移（重构架构、取消 patch、直接纳入代码）**——三阶段执行，每阶段工作如下：

| 阶段 | 工作内容 | 完成判据 |
| --- | --- | --- |
| 阶段 1 · 源码树导入 | 导入 `drivers/` 源码树；13 个补丁转为源码树上的直接提交；编写
  `binary-manifest.json` + `extract-vendor-binaries.sh`（幂等）；新构建流程（仓库树 +
  vendor/ 二进制），去掉 patch 应用 | `drivers/` 与 p27 源码逐字一致；提取工具就位且幂等；
  离线编译通过；功能回归 = p27 行为 |
| 阶段 2 · 版本独立与 CI | 独立版本号（`1.0.0-iN`），tag = 源码提交 + 清单哈希；
  `run-dev-tests.sh` 开发测试闭环门槛 | 每个提交跑静态 + 离线编译 + fixture 全绿；tag 可复现 |
| 阶段 3 · 持续迭代 | 在 `drivers/` 内做真正重构（invisible READ 预取、DPU 模块化）；
  以 `driver-architecture.md` 指导改动归属 | 重构项逐个设计 → 验证 → 合并，回归不破 |

详细设计见 [source-tree-migration.md](../planning/source-tree-migration.md)。

### 待办

6. **性能优化**：invisible READ 批量预取（候选 4，需先设计）；apphint 调优（候选 5）；
   上游 bugfix 移植（候选 6，依赖开源 DDK 可得性）；用户态调用画像（候选 8）；
7. **能力深挖**：DVFS/功耗实测与调参（候选 7）、CORE_ID/BVNC 直接读取、私有 codec 编码接口验证、
   WebKit DMA-BUF 上游修复报告；
8. **远期定向 RE**：`innogpu.o_shipped`（HAL）与 `innodma.o_shipped`（DMA）符号级分析，
   目标以"开源谱系 + 还原 HAL"组合替换预编译核心；
9. **发布收尾**：跨硬件实机矩阵（扩展坞/多屏/无盖桌面/其他机型）、电源/合盖矩阵、release 附件上传。

## 计划任务完整清单

唯一任务清单为 [todo.md](../planning/todo.md)，按方向组织：

- 文档与维护（回退演练、脚本收敛、发布一致性）
- WebKit DMA-BUF 调查（应用级 workaround 已定，上游报告待整理）
- 逆向工程与能力挖掘（普查 ✅、内核修复 ✅、剩余运行时项、DDK 对照 ✅、预取、符号级分析）
- 源码树迁移（阶段 1–3）
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
