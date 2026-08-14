# 文档入口

仓库根 [README](../README.md) 只给出当前结论和快速开始。本目录按“当前是什么、为什么这样、
如何验证、失败时怎么办”的顺序组织，维护时先看当前状态，再看架构约束和对应阶段记录。

## 推荐阅读顺序

1. [当前状态](project/status.md)：已安装版本、已解决问题、未解决问题和发布判断。
2. [项目架构](project/architecture.md)：驱动、用户态、显示、Picom、音频和目录边界。
3. [维护策略](project/maintenance-policy.md)：不可破坏的开发、隐私、测试和 release 约束。
4. [阶段补丁](patches/README.md)：每个补丁的目的、开关、验证和回退边界。
5. [事故与经验](incidents/README.md)：失败证据、根因、排除项和后续门槛。
6. [用户验证](user/verification.md)：安装或重启后的最小验收流程。
7. [术语表](project/glossary.md)：首次接手时需要的硬件、图形与打包缩写。

按需查阅：[依赖与外部文件](project/dependencies.md)、[显示接入使用](user/display-guide.md) 和
[实施历史](planning/history.md)。

代码入口索引：[`scripts/README.md`](../scripts/README.md) 记录稳定脚本、生命周期和风险；
[`tools/README.md`](../tools/README.md) 记录构建期变换与诊断探针；
[`tests/README.md`](../tests/README.md) 记录本仓库可重复测试边界。

## 目录职责

| 目录 | 内容 | 权威范围 |
| --- | --- | --- |
| `project/` | 当前架构、状态、依赖、组件边界和维护契约 | 当前实现与规则 |
| `patches/` | 与代码补丁一一对应的阶段说明 | 补丁设计与验证 |
| `incidents/` | 已定位事故和经验积累 | 失败过程与诊断边界 |
| `planning/` | 历史、活动 TODO、挂起项和迁移计划 | 后续工作状态 |
| `user/` | 安装、验证、显示使用、Picom 和恢复 | 面向操作者的步骤 |
| `archive/` | 不再变化但仍需追溯的旧记录 | 历史只读材料 |

同一事实只保留一个权威来源，其他文档使用链接。设计或计划必须明确标记“未实施”，不能与
当前已验证行为混写。文档优先、代码边界、测试和隐私规则见
[维护策略](project/maintenance-policy.md)。
