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
[当前待办](planning/current-work.md)、[已完成工作与时序](planning/todo.md)、[实施历史](planning/history.md)、
[逆向工程与能力挖掘评估](investigations/reverse-engineering-assessment.md)、
[FH2M 能力普查记录](investigations/capability-survey.md)、[release 审阅记录](planning/release-review-2026-08-20.md)、
[DDK V119 对照表](investigations/ddk-v119-mapping.md)、[源码树迁移](planning/source-tree-migration.md)、
[Phase 4 实机验证](planning/phase4-device-validation.md) 与 [Phase 5 退役设计](planning/phase5-retirement-design.md)。

调查与评估另见 [WebKit DMA-BUF](investigations/webkit-dmabuf-investigation.md)、
[Picom 接入记录](investigations/picom-integration.md)、[fantgpu 基座评估](investigations/fantgpu-base-update-evaluation.md)；
未完成但暂不推进的事项见 [挂起项](state/suspended.md)。

代码入口索引：[`scripts/README.md`](../scripts/README.md) 记录稳定脚本、生命周期和风险；
[`tools/README.md`](../tools/README.md) 记录构建期变换与诊断探针；
[`tests/README.md`](../tests/README.md) 记录本仓库可重复测试边界。

## 基线代际阅读入口

R17 第 2 轮草稿，待 qoder/dsh 审查。按本代工作与前代差异阅读；补丁、事故、设计和证据
仍保留原路径与权威范围。三代名称描述维护阶段，不是 Git 祖先链，也不指 `baselines/` 运行结果目录。

| 代际 | 阅读页 | 当前角色与边界 |
| --- | --- | --- |
| 代一 | [legacy patched 阶段（3.3.3.42-patched-N）](baselines/baseline-legacy-patched.md) | 原包解包+补丁叠加；p27 已属 Deepin，0.5 非 p27 祖先 |
| 代二 | [deepin 4.0.x 源码树迁移线](baselines/baseline-deepin-4.x.md) | 4.0.2-i3 为 Deepin 冻结终点与当前回滚卡 |
| 代三 | [fantgpu 5.0.0-iN（当前诊断线）](baselines/baseline-fantgpu-5.x.md) | F0 + 030 链；R5=FAIL、未签发、未打 tag |

历史页面里的“当前”只指其记录时点；今日结论统一回到 [status](project/status.md)。
新设备选择和回退步骤仍分别由 [安装指南](user/new-device-install.md)、[恢复规程](user/recovery.md)
管理，代际页面不授予执行许可。

## 目录职责

| 目录 | 内容 | 权威范围 |
| --- | --- | --- |
| `project/` | 当前架构、状态、依赖、组件边界和维护契约 | 当前实现与规则 |
| `baselines/` | 三篇基线代际阅读页 | 代际历史与差异；不同于仓库根 `baselines/` 运行结果 |
| `patches/` | 与代码补丁一一对应的阶段说明 | 补丁设计与验证 |
| `incidents/` | 已定位事故和经验积累 | 失败过程与诊断边界 |
| `investigations/` | 调查、评估、能力普查与历史接入记录 | 保留各文档的事实、计划与验证边界 |
| `state/` | 挂起项 | 暂不推进事项及其恢复条件；当前结论仍由 `project/status.md` 管理 |
| `planning/` | 当前待办、已完成时序、设计及冻结证据 | 迁移进行中；工具或冻结引用绑定的文件保留原位，`current-work.md` 管待办、`todo.md` 管已完记录 |
| `user/` | 安装、验证、显示使用、Picom 和恢复 | 面向操作者的步骤 |
| `archive/` | 不再变化但仍需追溯的旧记录 | 历史只读材料 |

根目录下 `docs/new-device-install.md` 与 `docs/cleanup-20260708.md` 是为旧链接保留的 compatibility
stub；权威内容分别位于 `docs/user/` 和 `docs/archive/`，不得在 stub 中复制或维护第二份正文。

同一事实只保留一个权威来源，其他文档使用链接。设计或计划必须明确标记“未实施”，不能与
当前已验证行为混写。文档优先、代码边界、测试和隐私规则见
[维护策略](project/maintenance-policy.md)。
