# 文档入口

仓库根 [README](../README.md) 只给出当前结论和快速开始。本目录按“当前是什么、为什么这样、
如何验证、失败时怎么办”的顺序组织，维护时先看当前状态，再看架构约束和对应阶段记录。

## 推荐阅读顺序

1. [整体目标与工作路线](project/goals.md)：为什么做、要达成什么、按什么顺序做。
2. [当前状态](project/status.md)：已安装版本、已解决问题、未解决问题和发布判断。
3. [术语表](project/glossary.md)：首次接手需要的硬件、图形与打包缩写；不熟悉缩写时先读此页。
4. [项目架构](project/architecture.md)：驱动、用户态、显示、Picom、音频和目录边界。
5. [代码深度分析](project/code-analysis.md)：4.0.0-i1 基线下的架构、构建/运行链、脚本质量（P0-P3）与黑盒/许可证边界。
6. [技术栈与参考模型](project/frameworks-and-references.md)：开发框架、来源基线、参考模型与证据等级。
7. [测试体系策略](project/test-strategy.md)：分层、能力域、输出规范与执行顺序。
8. [维护策略](project/maintenance-policy.md)：不可破坏的开发、隐私、测试和 release 约束。
9. [阶段补丁](patches/README.md)：每个补丁的目的、开关、验证和回退边界。
10. [事故与经验](incidents/README.md)：失败证据、根因、排除项和后续门槛。
11. [用户验证](user/verification.md)：安装或重启后的最小验收流程。
12. [多 Agent 协作规约](project/multiagent-collab.md)：dsh 与 codex 的协作流程、审查门禁、git 纪律与定期文档梳理（唯一权威，不复制规则）。

按需查阅：[依赖与外部文件](project/dependencies.md)、[显示接入使用](user/display-guide.md)、
[许可证与再分发边界](project/licensing.md)（唯一权威文档）、[驱动源码许可证审计](project/source-license-audit.md)、
[project-tools 允许清单](project/project-tools-allowlist.txt)、[driver-source 允许清单](project/driver-source-allowlist.txt)、
[当前待办](state/current-work.md)、[已完成工作与时序](history/todo.md)、[实施历史](history/history.md)、
[逆向工程与能力挖掘评估](investigations/reverse-engineering-assessment.md)、
[FH2M 能力普查记录](investigations/capability-survey.md)、[release 审阅记录](archive/release-review-2026-08-20.md)、
[DDK V119 对照表](investigations/ddk-v119-mapping.md)、[源码树迁移](design/source-tree-migration.md)、
[Phase 4 实机验证](archive/phase4-device-validation.md) 与 [Phase 5 退役设计](design/phase5-retirement-design.md)。

调查与评估另见 [WebKit DMA-BUF](investigations/webkit-dmabuf-investigation.md)、
[Picom 接入记录](investigations/picom-integration.md)、[fantgpu 基座评估](investigations/fantgpu-base-update-evaluation.md)；
未完成但暂不推进的事项见 [挂起项](state/suspended.md)。

代码入口索引：[`scripts/README.md`](../scripts/README.md) 记录稳定脚本、生命周期和风险；
[`tools/README.md`](../tools/README.md) 记录构建期变换与诊断探针；
[`tests/README.md`](../tests/README.md) 记录本仓库可重复测试边界。

设计与计划按主题查阅：[R5 调查方案](design/r5-suspend-investigation-plan.md)、
[本机 watchdog 抓现场](design/r5-watchdog-local-capture-design.md)、
[r5dpm1 诊断内核](design/r5-dpm-watchdog-diagnostic-kernel-design.md)、
[r5dpm2 诊断内核](design/r5-dpm-prepare-watchdog-diagnostic-kernel-design.md)、
[VPU契约修正与R5观测实现](design/r5-vpu-and-observation-implementation.md)；
[O_stage 集成](design/o-stage-integration-plan.md)、[可复现输入](design/c3-a-4-reproducible-input-plan.md)、
[5.0.0-i2 验证计划](design/5.0.0-i2-validation-plan.md)；
[030 关闭项裁决](design/030-closed-items-decision.md)、[D_stage 审计契约](design/030-d-stage-audit.md)、
[补丁溯源](design/patch-provenance.md)。设计页保留各自记录时点的边界，不授予执行许可。

## 基线代际阅读入口

三篇于 R17 第 2 轮完成审查并提交（1bf294a），R18 批 1 迁入本目录。按本代工作与前代差异阅读；补丁、事故、设计和证据
保留各自权威范围，路径按导航查阅。三代名称描述维护阶段，不是 Git 祖先链，也不指 `baselines/` 运行结果目录。

| 代际 | 阅读页 | 当前角色与边界 |
| --- | --- | --- |
| 代一 | [legacy patched 阶段（3.3.3.42-patched-N）](baselines/baseline-legacy-patched.md) | 原包解包+补丁叠加；p27 已属 Deepin，0.5 非 p27 祖先 |
| 代二 | [deepin 4.0.x 源码树迁移线](baselines/baseline-deepin-4.x.md) | 4.0.2-i3 为 Deepin 冻结终点与当前回滚卡 |
| 代三 | [fantgpu 5.0.0-iN（当前诊断线）](baselines/baseline-fantgpu-5.x.md) | F0 + 030 链；R5=FAIL、未签发、未打 tag |

历史页面里的“当前”只指其记录时点；今日结论统一回到 [status](project/status.md)。
轮次阅读版：[R01–R16 批量索引](history/history.md#轮次公开阅读版索引)。
R01 试点已审，R02–R16 本批待审；各页区分历史结论与当前冻结状态。
阅读版提供公开可读的事实与边界；原文和快照仍在本机，不随 Git 分发。
新设备选择和回退步骤仍分别由 [安装指南](user/new-device-install.md)、[恢复规程](user/recovery.md)
管理，代际页面不授予执行许可。

## 目录职责

| 目录 | 内容 | 权威范围 |
| --- | --- | --- |
| `project/` | 当前架构、状态、依赖、组件边界和维护契约 | 当前实现与规则 |
| `baselines/` | 三篇基线代际阅读页 | 代际历史与差异；不同于仓库根 `baselines/` 运行结果 |
| `patches/` | 与代码补丁一一对应的阶段说明 | 补丁设计与验证 |
| `incidents/` | 已定位事故和经验积累 | 失败过程与诊断边界 |
| `design/` | 设计、集成方案、验证计划与配套契约 | 设计及其历史裁决；当前状态和执行放行另行确认 |
| `investigations/` | 调查、评估、能力普查与历史接入记录 | 保留各文档的事实、计划与验证边界 |
| `state/` | 当前待办与挂起项 | 未完成事项及恢复条件；当前结论仍由 `project/status.md` 管理 |
| `history/` | 已完成时序、实施历史与显示接入历史 | 记录当时的原因、结果与证据；不授予执行许可 |
| `planning/` | 四篇 meta 绑定文档及冻结证据 | 冻结引用与证据原路径保留；归档候选保留兼容跳转 |
| `user/` | 安装、验证、显示使用、Picom 和恢复 | 面向操作者的步骤 |
| `archive/` | 不再变化但仍需追溯的旧记录 | 历史只读材料 |

状态与时序的分工：[status](project/status.md) 管当前结论，
[current-work](state/current-work.md) 管待办，[suspended](state/suspended.md) 管暂停条件，
[todo](history/todo.md) 是已完成事项索引，[history](history/history.md) 解释关键演进及其证据。
工具适配批将 current-work 归入 state，todo/history 与 [显示接入历史](history/display-integration.md)
归入 history，两篇源码迁移/退役设计归入 design；检查脚本跟随新路径，原有检查继续有效。
planning 保留四篇 meta 引用目标和 evidence 冻结区；R20 的 D2 试点与 D4 归档均已闭合，
R21 承接 R02–R16 阅读版批量提升，整批放行前仍须逐篇隐私核验和内容审查；
目录迁移不代表运行问题闭合。
archive 现有 [2026-07-08 清理记录](archive/cleanup-20260708.md) 仅代表当时状态；
[Phase 4 验证](archive/phase4-device-validation.md) 与 [release 审阅](archive/release-review-2026-08-20.md)
已按 D4 归档；旧 planning 路径仅保留兼容跳转页。
R18 的 D4 归档条件为：无当前入口职责、有替代或已结案且台账可追溯、不在冻结区且不被冻结指针引用、
旧路径留跳转且原文保全、单独审批不与迁移混批。未满足前只登记候选。

根目录下 `docs/new-device-install.md` 与 `docs/cleanup-20260708.md` 是为旧链接保留的 compatibility
stub；权威内容分别位于 `docs/user/` 和 `docs/archive/`，不得在 stub 中复制或维护第二份正文。

同一事实只保留一个权威来源，其他文档使用链接。设计或计划必须明确标记“未实施”，不能与
当前已验证行为混写。文档优先、代码边界、测试和隐私规则见
[维护策略](project/maintenance-policy.md)。
