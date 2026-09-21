# R01 · 文档梳理（2026-08-31，公开阅读版）

- 记录范围：2026-08-31 的盘点、用户决策、执行及终审；历史提交 `301d62c09cbb`。
- 参与方：dsh 监督、codex 实现，用户对本轮历史 D1-D4 拍板。
- 阅读版整理：2026-09-21，R20 的 D2 试点；本页不替代[当前状态](../../project/status.md)。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R01-2026-08-31-文档梳理/narrative.md`（2026-09-19 样本），内容 SHA-256：
  `0ce5268365d58eb8158d3e6f3a416b71584804528a66f24f0c76d90c1b29cae4`。

脱敏与编辑差异清单（相对上述来源版本）：

1. 不复制私有核对表及内部审查模板指引；将 E1-E8 的事实、处置、教训补齐为本文摘要。
2. 本机来源只保留路径、内容哈希与行号标识，明确不可随 Git 取得，不伪装为公开附件链接。
3. 增加可在 Git 历史核验的提交锚，以及当前 tracked 文档导航；不替换历史哈希或裁决值。
4. 按原始终审记录明确 t03 最终采用隔离 fixture，并非仍待选择“隔离或删除”。
5. 区分历史验证与本次阅读版检查；定期梳理规约在盘点基线已存在，R01 是首个执行实例。

来源未检出共享隐私模式命中；本次未替换事件数值、失败记录或裁决语义。

## 一、解决什么问题

三层许可证模型、发布决策 1C、协作角色与本机私有 collab 机制陆续落地后，文档与实现
发生漂移。R01 对全库 76 个受跟踪 Markdown 做“实现 → 文档、文档 → 实现”双向盘点，
检查状态、许可证、测试契约、待办与导航；不能只检查协作规约自身。

实际盘点基线为 `HEAD=13b5fbb`、tracked 711、Markdown 76。该提交已规定每 5-6 轮或
重大调整后梳理文档，R01 将规则付诸第一次完整执行；计数只描述当时仓库。

## 二、怎么干的

先只读盘点，列出双向对照、13 项冲突/结构问题和四项需用户决定的事项；dsh 核验、用户
批准后才修改。每处修改记录“事实依据 → 文档修改”，以实现、CI 与 runner 为事实源。
除 E1-E8 外，同批还补齐 collab 目录边界、工具职责、协作术语和根导航，避免新机制没有入口。

公开导航只连接 tracked 文件。下表的链接指向当前文件，版本锚则锁定历史提交内的
文件与行号；历史路径以提交读取，不要求它仍在当前目录存在。

| 可公开查阅的对象 | 历史版本锚 | 可核验内容 |
|---|---|---|
| [许可证测试](../../../tests/unit/run-license-audit-tests.sh) | `tests/unit/run-license-audit-tests.sh#L418-L420 @ 13b5fbb24416` | 修复前 t03 依赖真实工作树状态 |
| [许可证测试](../../../tests/unit/run-license-audit-tests.sh) | `tests/unit/run-license-audit-tests.sh#L418-L428 @ 301d62c09cbb`；`tests/unit/run-license-audit-tests.sh#L636-L647 @ 301d62c09cbb` | 隔离 fixture 与保留的两条脏树反例 |
| [当前待办](../../state/current-work.md) | `docs/planning/current-work.md#L1-L18 @ 301d62c09cbb` | 当时新增唯一待办入口、发布条件项与研发验证分离 |
| [文档门禁](../../../scripts/check-docs.sh) | `scripts/check-docs.sh#L357-L378 @ 301d62c09cbb` | 必需入口及 todo 活动 checkbox 拒绝规则 |
| [测试策略](../../project/test-strategy.md) | `docs/project/test-strategy.md#L7-L17 @ 301d62c09cbb` | unit 366、全套 385 的历史单点汇总 |
| [工具契约](../../../tools/README.md) | `tools/README.md#L22 @ 301d62c09cbb` | VA-API rc=124/137 统一 timeout 契约 |

本机来源（以下全部**非 Git、公开检出不可取得**；标识用于本机复核，不是公开下载链接）：

| 标识 | 路径与内容 SHA-12 | 用途 |
|---|---|---|
| S0 | `collab/R01-2026-08-31-文档梳理/narrative.md @ 0ce5268365d5` | 本页来源版本，完整 SHA 见文首 |
| S1 | `collab/R01-2026-08-31-文档梳理/report.md @ f2776df3a713` | #L5-L52 盘点；#L102-L131 决策授权；#L135-L213 执行与终审 |
| S2 | `collab/R01-2026-08-31-文档梳理/request.md @ 0269f43db58a` | 当时要求及请求基线 |
| S3 | `.runtime-archive/r17-docs/archive-originals/R01/R01-originals.tar @ 30c465714904` | 原文快照；内含 `./report.md` 与 `./request.md`，行号锚按快照读取 |
| S4 | `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R01-sample.md @ 5a72871908d6` | 补齐事件三要素的来源；未将整套核对表公开 |

## 三、发现了什么问题（错误资产）

E 编号沿用来源，位置均为 S3 内 `./report.md` 行号；表内已给出理解事件所需事实，
公开读者无需取得 S1-S4 才能阅读。引用旧文档行号的数值也只指当时版本。

| 事件与来源位置 | 事实 | 处置 | 教训 |
|---|---|---|---|
| E1 · #L18 | license t03 依赖真实工作树必须为脏并期望 rc=2；干净 HEAD 实测 49/50（FAIL），隔离脏树反例 t31/t32 仍 PASS；是“测试环境耦合，不是发布门禁失效” | 用户单独批准最小测试修复；t03 改隔离 fixture，断言整仓 BLOCKED + project-tools CLEARED，保留 t31/t32；原记录报告修复后 50/50 | 可重复测试不能依赖开发者真实工作树是否干净；修测试不能放宽发布门禁 |
| E2 · #L17、#L33、#L108-L109 | test-strategy 内部 385/366 与 357/48 并存，VA-API 52/56 并存；tests/README:66 许可证审计仍写“11 项”，实际 50 | 用户 D3 收敛为顶部唯一汇总（unit 366、全套 385），其他章节与 README 只述契约；同步清理“11 项” | 快速变化的计数不可多点维护；runner 运行时汇总是机械事实 |
| E3 · #L16 | tools/README:22 仍把 VA-API rc=137 写成“当前已知实现缺口”，但实现已统一 rc=124/137，fixture 56 项 | 删除过时缺口描述，准确记录三阶段 timeout 契约与退出码 5 | 文档缺陷清单须随实现和测试结论更新，不能把已落实修复写成仍待修复 |
| E4 · #L72 | request 声称 HEAD=d00740a，实盘却为 13b5fbb，后者新增了定期梳理规则 | 采用真实干净 HEAD=13b5fbb 盘点并保留请求基线漂移记录；Git 可核验 d00740a → 13b5fbb → 301d62c 的直接父子关系 | 提示词中的版本承诺必须先与实物核对，不能静默改写原请求 |
| E5 · #L44 | status:97、code-analysis:105,135、todo:53-56 仍把 check-docs 全 Markdown 扩展列为未完成，实现已使用 git ls-files 全量枚举 | 修正当前状态、源码分析与计划文档，scripts/README 同步覆盖边界 | 状态须随实现更新，不能沿用复合 TODO 中已失效的子项 |
| E6 · #L45 | README:26 的“drivers/ 自有源码树”与导入厂商源码、逐文件许可事实冲突 | 改为“仓库内维护的导入源码树”，不改变许可证状态 | 目录由本仓库维护不等于拥有源码版权，表述必须符合许可边界 |
| E7 · #L46 | README 与 status 日期同为 2026-08-27，已漏记后续变更；日期相等门禁仍通过 | 依据当时实现同步日期为 2026-08-31，并更新状态说明 | 双文件共同过期也能一致，日期相等不能替代内容时效核查 |
| E8 · #L41 | licensing §4.1 的“1C 下不创建 tag/Release/附件”与 todo:68“release 附件上传”活动项冲突 | 用户 D1 关闭发布活动项，将跨硬件/电源矩阵改为研发验证项；仅未来明确推翻 1C 才重新激活发布任务 | 发布决策语义必须同步到活动 TODO；不能保留相反操作作为默认下一步 |

## 四、怎么解决的

这里的 D1-D4 是 **R01 当时的四项决策**，不是 R20 的 D2 叙事提升/D4 归档批号。
用户全部采纳首选方案后，才授权第二阶段：D1 收敛发布待办；D2 分离当前待办与已完时序；
D3 收敛计数权威；D4 单独准许最小修复测试。最终 t03 使用隔离 fixture，未删除 t31/t32。

提交 `301d62c09cbb` 收齐 19 件、+166/-139；allowlist 从 212 增到 213，仍为 0 条 collab。
这些变更可由 Git 核验。原始执行及终审记录（S1 #L176-L213）报告 unit 366/366、全套
385/385、license 50/50、collab 26/26、`RESULT: PASS_DOCS`，审计保持
PASS/BLOCKED/CLEARED/BLOCKED。这些是 **2026-08-31 的历史验证记录，本次未重跑历史套件**；
公开 Git 可验证当时实现及文档，不能仅凭该提交把本机报告中的执行结果当作新的实测。

当前入口已在 R19 迁移为 [current-work](../../state/current-work.md)、
[todo](../todo.md) 与 [history](../history.md)。正文保留 R01 当时的路径和版本身份；
今日任务、规约与许可结论分别以当前待办、[协作规约](../../project/multiagent-collab.md)、
[许可证边界](../../project/licensing.md) 为准，不在历史页维护第二份现状。

## 五、特别说明

- R01 未授权改动 drivers/、baselines/、binary-manifest.json 或监督分支
  migration/supervised-source-tree；历史审阅文字与发布决策 1C 语义保留。
- R01 的完成仅覆盖当时梳理与获准的最小测试修复；研发矩阵、音频生命周期、Debian
  最小构建依赖及 vendor service 生命周期仍是后续事项。project-tools CLEARED 不等于
  整仓可发布，release 与 driver-source 的 BLOCKED 未被本轮工作解除。
- 本次仅提升 R01 脱敏阅读版，不公开原始记录、快照或整套私有核对表，不代表其他轮次获准提升。
  哈希只标识来源版本；历史命令、历史审批和历史 PASS 均不构成当前执行授权。
- 当前冻结不变：OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2 未执行、validation-results
  未签、签发与 tag 冻结。R01 历史结论不覆盖后来出现的 R5 悬案。
