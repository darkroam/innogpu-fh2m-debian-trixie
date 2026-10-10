# 项目目录使用规范

本页定义仓库目录的用途、写入边界、Git 状态和生命周期。它约束未来任务从哪里读取输入、把
构建与证据写到哪里，以及何时可以清理；不授予安装、运行测试或删除冻结证据的权限。

## 核心原则

- 源码、补丁、脚本、测试和正式文档才进入 Git；外部包、二进制载荷、构建树和本机证据不入库。
- 不可变输入与可重建工作树分离：输入包放 `debs/`，展开源码和对象放 `.build/work/`。
- 构建工作树与验证证据分离：前者闭链后可清，后者放 `.build/evidence/` 并按轮次裁决。
- 运行期原始采集放 `.runtime-archive/`，默认冻结只读；删除或迁移必须逐批列清单并由用户确认。
- `build/` 是已退役的 R3–R16 遗留混合区；R50 已将内容分类迁出并删除空根，禁止复生且不建兼容 symlink。
- 内核升级后保留当前版本源码包；只有新版本稳定且旧版无待研究补丁时，才清理前一版本源码包。

## 两线

- **F 线**是唯一开发主线。源码与黑盒载荷都来自
  `debs/fantgpu-fh2m_3.3.8.126-driver-linux-desktop-sp-generic_amd64.deb`
  （SHA-256 `6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775bebc2d28fd11b`）：
  F0 genesis、R16 整栈切换、冻结 O-stage 快照，再加上 030-035/030-036。
  载荷清单是 `binary-manifest-fantgpu.json`，物化目录是 `vendor/fantgpu/`。
- **O 线**只维护，不接新功能。主线不含 `drivers/`。484 件在 tag `innogpu-4x-frozen`；
  相对 `deepin-4.0.2-i3` 只有 `drivers/README.md` 四行路径表述不同。构建器在主线拒绝 O 分支，
  原句是「O 线仅从 deepin-4.0.2-i3 tag 检出构建」。载荷 deb
  `debs/innogpu-fh2m_20250421190503-debug_amd64.deb`
  （SHA-256 `b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2`）仍保留；
  `binary-manifest.json` 与 `tools/extract-vendor-binaries.sh` 不在主线，可从 `innogpu-4x-frozen` 取回。
- 两线输入不混用。

## 目录职责

| 目录 | 用途与写入者 | Git 状态 | 生命周期与清理条件 |
|---|---|---|---|
| `.github/` | CI 与仓库托管配置 | tracked | 随代码审查维护 |
| `LICENSES/` | 项目使用的标准许可证文本 | tracked | 许可证策略变更时同步 |
| `components/` | 第三方组件的固定来源说明、最小 patch、meta 与静态回归 | tracked | 随上游版本升级审查；不放展开源码 |
| `drivers/` | O 线源码；主线不含 | 不在主线；tag `innogpu-4x-frozen` 可取回 484 件 | 不在主线重建。构建只从 `deepin-4.0.2-i3` 检出 |
| `patches/` | 历史/当前驱动补丁及溯源材料 | tracked | 已迁入源码的补丁仍保留作复现锚 |
| `tools/` | 操作者与 CI 的稳定命令入口 | tracked | 在 `tools/README.md` 登记 |
| `tools/internal/` | 构建期变换、审计与诊断；不是给人记的命令名 | tracked | 门禁与测试直接调用；与 allowlist 同步 |
| `tests/` | fixture、静态与单元回归 | tracked | 与对应行为同批更新 |
| `docs/` | 当前状态、设计、事故、用户指南和历史 | tracked | 同一事实只设一个权威页 |
| `third_party/` | 可入库的第三方源码/说明；指定的大型解包目录除外 | 混合 | 每项必须有来源、版本、许可和适用条件；解包树由 `.gitignore` 排除 |
| `vendor/` | manifest 管理的本地二进制 payload 物化区 | ignored | 不入库；可由锁定输入恢复后才可重建/清理 |
| `debs/` | 包管理或人工取得的不可变 `.deb`/源码包/回滚实物 | 内容 ignored，`README.md` tracked | 当前内核版源码包和有效回滚包保留；升级稳定后再清前版 |
| `.build/work/` | 展开源码、对象、stage、临时构建与可重建工具 | ignored | 对应任务闭链且权威输入存在后可清 |
| `.build/evidence/` | 本机 SHA、日志、回执、安装和验收证据 | ignored | 按轮次保留/清理裁决；禁止与 work 一起整根删除 |
| `.build/tools/` | 可重建工具展开树与 cache | ignored | 原始 archive 完整且校验通过后可清 |
| `.build/r*-*` | 既有轮次目录 | ignored | 依目录内 work/evidence 分类，不能仅凭顶层名称整根删除 |
| `.runtime-archive/` | watchdog、启动、PM 与事故现场原始采集 | ignored、冻结 | 默认只读；删除/迁移需精确清单、SHA 和用户确认 |
| `build/` | 已退役的 R3–R16 旧混合区 | ignored、禁止复生 | R50 已迁空并删除；check-docs 对目录重新出现 fail-closed |
| `baselines/` | 精简运行基线和 PASS/FAIL marker | 混合 allowlist | 原始日志转本地证据区，只跟踪已定义 marker |
| `collab/` | 多 Agent 轮次请求、流水、审查与交接 | ignored、本机私有 | 按协作规约保留；不得混入非 Markdown payload |
| `.agents/`、`.codex/`、`.qoder/` | Agent/工具本机会话状态 | ignored | 不属于项目交付，可由对应工具管理 |
| `.shaders/` | 驱动生成的 shader cache | ignored | 可随时清理 |
| `.git/` | Git 内部数据库 | Git 自管 | 禁止由项目清理脚本操作 |

`.build/work/`、`.build/evidence/` 和 `.build/tools/` 是当前布局；既有轮次目录可继续存在，
但所有新脚本必须采用目标布局。证据目录名称必须包含轮次或稳定主题，不能使用无语义的
`tmp` 作为长期位置。

## `build/` 合并方案

R50 没有把 `build/` 整体改名为 `.build/`，而是按内容完成分类迁移：

1. R16 展开输入和可重建工具已迁到 `.build/work/r16/`。
2. R16 与 R3–R14 的日志、观测和清单已迁到 `.build/evidence/r16/` 或
   `.build/evidence/legacy/`。
3. 需要保留的 14 个 `.deb` 已迁到 `debs/archive/legacy-build/`。
4. 迁移使用逐项 source→target、type、size、content/metadata SHA manifest，并经物理 TTY 第二次确认。
5. 全库现行消费者已更新，43 个目标逐项核对 SHA，四门禁与相关 R16 回归通过。
6. 空 `build/` 已删除，不创建兼容 symlink；ignore 与 check-docs 继续防止复生和误提交。

R16 的 `p2-manifest.tsv` 严格区分生成态与接受态：`p2-normalize-v3.py` 默认写
`.build/work/r16/p2-manifest.tsv`，production `r16-gate.py` 默认只读
`.build/evidence/r16/p2-manifest.tsv`；生成器不得覆盖接受证据。BC map/classify 的生成态同样写
`.build/work/r16/r16-evidence/`，production gate 只读 `.build/evidence/r16/r16-evidence/`。

未来候选包先写 `.build/work/<轮次>/packages/`；只有被裁决为长期输入或回滚实物后，才显式归档到
`debs/archive/<轮次或用途>/`。`build/` 迁空后不建兼容 symlink；ignore 规则作为防误提交兜底保留，
文档门禁负责拒绝该目录重新出现。

R50 的 45 项动作记录在本机轮次的 `task4-migration-plan.tsv`；绑定 TTY 回执、操作日志和迁移后
逐项校验位于同轮 `task4-migration/`。映射表本身仍只是计划，是否执行以 PASS 结果和后验为准。

## 新任务应放到哪里

- 新内核 patch：源码、meta、测试放 `components/linux/`；源码 `.deb` 放 `debs/`；展开树放
  `.build/work/<round>/`；构建与运行结果放 `.build/evidence/<round>/`。
- 第三方软件 patch：放对应 `components/<name>/`，同时记录适用版本、构建前置、是否需要完整
  源码、部署/回退和升级检查；不要把上游完整源码复制进仓库。
- 事故现场：先放 `.runtime-archive/`，闭合后将可公开的结论写入 `docs/incidents/`，原始现场
  仍保持 ignored。
- 一次性实验：放 `.build/work/`；只有被裁决为证据的结果再写入 `.build/evidence/`。

## Git 防误入库检查

提交前至少运行：

```bash
git check-ignore -v --no-index .runtime-archive/example .build/example build/example \
  debs/example.deb vendor/example collab/example .agents/example .codex/example .qoder/example
git status --short --untracked-files=all
tools/check-docs.sh
```

若应忽略路径未命中规则，先修正 `.gitignore`；不得依赖“当前目录为空所以 status 看不到”。
