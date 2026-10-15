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
  F0 genesis、R16 整栈切换、冻结 O-stage 快照，再加上 m-035-private-accessor/m-036-bf-shf-rename。
  载荷清单是 `binary-manifest-fantgpu.json`，物化目录是 `vendor/fantgpu/`。
- **O 线**只维护，不接新功能。主线不含 `drivers/`。484 件在 tag `innogpu-4x-frozen`；
  相对 `deepin-4.0.2-i3` 只有 `drivers/README.md` 四行路径表述不同。构建器在主线拒绝 O 分支，
  原句是「O 线仅从 deepin-4.0.2-i3 tag 检出构建」。载荷 deb
  `debs/innogpu-fh2m_20250421190503-debug_amd64.deb`
  （SHA-256 `b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2`）仍保留；
  `binary-manifest.json` 与 `tools/extract-vendor-binaries.sh` 不在主线，可从 `innogpu-4x-frozen` 取回。
- 两线输入不混用。

## 目录职责

每个二级条目五列：用途、读写者、跟踪、生命周期、冻结。冻结写「否」或冻结原因。没有二级子目录的，写本目录用法。

### 根文件

根上只留门面和身份，不新增第二层。

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `README.md` | 入口。指向 status 与本页 | 文档批 | 是 | 与 status 同日更新 | 否 |
| `LICENSE` | 原创层 GPL-3.0-or-later | 许可批 | 是 | 许可策略变更时 | 文本冻结 |
| `THIRD_PARTY_NOTICES.md` | 第三方声明 | 许可批 | 是 | 新第三方路径同批 | 否 |
| `license-audit-policy.json` | 许可分类策略 | audit | 是 | 与 inventory 同批 | 否 |
| `binary-manifest-fantgpu.json` | F 线载荷清单 | 构建器只读 | 是 | 载荷变更才改 | 是，权利链未闭 |

### `.github/`

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `.github/workflows/` | CI。现只 `ci.yml` | 仓库维护 | 是 | 路径或门禁变更时 | 否 |

### `LICENSES/`

无子目录。标准许可证文本。audit 按路径组引用。不放项目说明。

### `components/`

软件在上，系统在下。先问改哪个软件，再问只服务哪个系统。跨系统操作不进系统子层。

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `components/picom/` | Picom 补丁、配置、`build.sh`、`install-user.sh`、`session.sh` | 组件维护 | 是 | 上游 commit 变了才改 | 补丁字节随审查 |
| `components/picom/debian/` | 只服务 Debian 的 `install-prereqs.sh` | Debian 安装 | 是 | 依赖变了才改 | 否 |
| `components/fbterm/` | fbterm 补丁与 `build.sh` | 组件维护 | 是 | Debian fbterm 1.7-5 基线变了才改 | 补丁字节随审查 |
| `components/linux/` | 内核补丁 001–004。适用内核以 [系统×内核矩阵](../patches/system-kernel-matrix.json) 为准 | 内核补丁维护 | 是 | 新内核必须重验，不得套旧模块 | 补丁字节随审查 |

### `drivers/`

主线无此目录。O 线 484 件在 tag `innogpu-4x-frozen`，构建只从 `deepin-4.0.2-i3` 检出。不在主线重建。

### `patches/`

无子目录。文件名是第二层：`m-` 必须打上，`g-` 胶水。计数以 `docs/patches/naming.md` 为准。系统×内核的当前表和进行中表在 `docs/patches/system-kernel-matrix.json`，新增行规程在 naming.md。D 线数字名 patch 留作 meta 对照，不改字节。不把组件补丁放回这里。

### `tools/`

顶层只留 `check-docs.sh`、`materialize-o-stage.sh`、`README.md`。系统操作不放顶层。

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `tools/debian/` | Debian 的 check/build/install/verify/rollback 与驱动自身脚本 | Debian 维护 | 是 | 与 Gentoo 同名动作对齐 | 否 |
| `tools/gentoo/` | Gentoo 同名动作，外加本机观察脚本 | Gentoo 维护 | 是 | 同上 | 否 |
| `tools/gentoo/overlay/` | portage overlay。其下是 ebuild 类别，不是本仓库的第二层动作名 | emerge | 是 | ebuild 与仓库脚本同提交 | 否 |
| `tools/internal/` | 不是给人记的命令名。门禁和测试直接调用 | 门禁 | 是 | 与 allowlist 同步 | 否 |

`tools/gentoo/overlay/` 现有 `metadata/`、`profiles/`、`licenses/`、`x11-drivers/fantgpu-fh2m/`、`sys-kernel/fantgpu-kernel-patches/`。新 ebuild 进对应类别，不在 `tools/gentoo/` 根再造一套。

### `tests/`

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `tests/unit/` | 无设备 fixture | 对应行为同批 | 是 | 行为变了就改 | 否 |
| `tests/runtime/` | 真机能力脚本。默认只读，授权项另说 | 授权后运行 | 是 | 与 runtime marker 同批 | 否 |
| `tests/package/` | deb 边界 | 打包变更同批 | 是 | 同上 | 否 |
| `tests/picom/` | Picom 安装与会话 | 组件同批 | 是 | 同上 | 否 |
| `tests/fbterm/` | fbterm 静态 | 组件同批 | 是 | 同上 | 否 |
| `tests/linux/` | 001/002 补丁形状 | 内核补丁同批 | 是 | 不代替拔插 | 否 |
| `tests/xdisplay/` | 显示接入安装 | 接入变更同批 | 是 | 引擎不入库 | 否 |

`tests/README.md` 留在 `tests/` 根。

### `docs/`

同一事实只设一个权威页。`docs/` 根只留 `README.md`。现有 `new-device-install.md` 与 `cleanup-20260708.md` 是历史散文件，本段不搬。

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `docs/project/` | 现状、架构、许可、目录、规约 | 文档批 | 是 | status 是运行状态唯一摘要 | 规约除授权节外只读 |
| `docs/user/` | 安装、验证、恢复、Debian、Gentoo | 用户文档 | 是 | 命令必须指向现存脚本 | 否 |
| `docs/design/` | 未过期的设计 | 设计批 | 是 | 执行后改状态，不删设计 | 否 |
| `docs/patches/` | 补丁说明、验收记录、系统×内核矩阵 | 补丁批 | 是 | 与 `patches/` 文件名同步；矩阵随验收行更新 | 历史验收不改结论 |
| `docs/incidents/` | 事故 | 事故闭合时 | 是 | 只追加结论 | 已闭合正文不改 |
| `docs/history/` | 当时记录。`rounds/` 是轮次叙事 | 不改当时事实 | 是 | 不把现在时写进旧轮 | 是 |
| `docs/investigations/` | 调查笔记 | 调查批 | 是 | 现行路径可改，调查事实不改 | 否 |
| `docs/archive/` | 已完结阶段的阅读页 | 只读引用 | 是 | 路径可改，结论不改 | 结论冻结 |
| `docs/baselines/` | 三代事实表。不是运行 marker | R17 已审 | 是 | 事实表不在本段改 | 事实表冻结 |
| `docs/state/` | `current-work.md`、`suspended.md` | 当轮 | 是 | 不与 status 争权威 | 否 |
| `docs/planning/` | 进行中的设计与映射 | 设计批 | 是 | 闭合后不把证据搬出 evidence | 否 |
| `docs/planning/evidence/` | 阶段证据。含 `o-stage/`、`4.0.2-i3/`、d-stage 清单 | 物化工具 | 是 | 不重写哈希 | 是 |

### `baselines/`

运行结果。不是 `docs/baselines/`。

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `baselines/current.txt` | 现行指针。只指 status，不造 PASS | 本段写入，之后只在有新捕获时改 | 是 | 有 i12 的 35 项捕获才改内容 | 否 |
| `baselines/runtime/` | 35 项摘要与 `runtime-results-20260824.txt`。带日期的 `runtime-baseline-*.txt` 不跟踪 | 能力脚本 | 摘要跟踪，原始 ignore | 新捕获不覆盖 4.0.0-i1 文件 | 已跟踪字节冻结，除本段路径迁移 |
| `baselines/capability/` | survey 日志 | survey 脚本 | 否 | 可清，不入库 | 否 |
| `baselines/desktop-hwgl/` | 一行 PASS、xorg/ddx 的 `result.txt`、已跟踪的 post-reboot 捕获 | HWGL 脚本 | marker 跟踪，原始 ignore | 脚本可重写一行 marker。已跟踪捕获不改字节 | 捕获冻结 |
| `baselines/phase4/` | 4.0.0-i1 的 phase4 结论与一份审计日志 | 不再生成 | 是 | 只证明该版本 | 是 |
| `baselines/legacy/` | patched-17 的 `final-summary.md` | 不再生成 | 是 | 不引用为新包证据 | 是 |

### `debs/`

无跟踪子目录。只跟踪 `README.md`。本地可以有 `archive/`，不进 git，不随仓库分发。输入包 SHA 以 README 和 manifest 为准。

### `vendor/`

整目录 ignore。本地物化可见 `firmware/`、`opt/`、`kernel/`。这不是跟踪布局。重建走 manifest，不手改字节。

### `third_party/`

整目录原则上 ignore。D 线解包树不在主线工作树。重建用 `dpkg-deb -x`，与 tag 上的 `materialize-d-stage.sh` 分成两段。

### `collab/`

ignore，本机私有。二级是 `R{编号}-{日期}-{主题}/`。只放 Markdown。不进发布。

### 本地区

| 路径 | 用途 | 读写者 | 跟踪 | 生命周期 | 冻结 |
|---|---|---|---|---|---|
| `.build/work/` | 展开与临时构建 | 脚本 | 否 | 任务闭链后可清 | 否 |
| `.build/evidence/` | 本机回执与日志 | 脚本 | 否 | 按轮次裁决，不与 work 整根删除 | 否 |
| `.build/tools/` | 工具展开 | 脚本 | 否 | archive 完整后可清 | 否 |
| `.runtime-archive/` | 现场原始采集 | 只读 | 否 | 删除要清单和 SHA | 是 |
| `build/` | 已退役 | 无 | 否 | 禁止复生 | 是 |
| `.agents/` `.codex/` `.qoder/` | 工具会话 | 工具 | 否 | 不属于交付 | 否 |
| `.shaders/` | 驱动 shader cache | 驱动 | 否 | 可清 | 否 |

## 本地输入与 D 线解包树

`debs/` 是本地输入仓库。除 `README.md` 外不进 git，也不随仓库分发。D 线旧 deb、
`innogpu-fh2m_20250421190503-debug_amd64.deb` 与 F 线输入包都留在本机。公开分发另走发布审阅。

`third_party/innogpu-fh2m-deepin-202504/` 已被 `.gitignore` 排除，不在任何 tag。它是上述 D 线 deb 的解包树，
不是 `materialize-d-stage.sh` 的产物。主线工作树不保留这棵树。重建：

```bash
mkdir -p third_party/innogpu-fh2m-deepin-202504
dpkg-deb -x debs/innogpu-fh2m_20250421190503-debug_amd64.deb \
  third_party/innogpu-fh2m-deepin-202504/root
```

输入 SHA-256 是 `b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2`。
哨兵 `usr/src/innogpu-kernel-2.2/Makefile` 与包内同路径一致，SHA-256
`9e35f6f8dfaac015210b1704daca6cda1b8ff27e07c20e41109bd79b597bb68d`。

tag `innogpu-4x-frozen` 的 `tools/materialize-d-stage.sh`（SHA-256
`428d5f7bcb167c9d8f7da52e6c789e77888a260509c3896e40cfbfed81ace0ed`）只读
`third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2`，写出 `/tmp/r16-d-stage`。
它不能代替上面的解包。

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
