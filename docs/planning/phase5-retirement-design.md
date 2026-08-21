# Phase 5 旧流程退役设计（两步方案，监督 2026-08-21 建议）

**状态**：设计评审中。Phase 5 未执行；不移动、不删除任何旧文件。设备已运行 `4.0.0-i1`
（Phase 4 完成），main 已合并并推送（74b1f04）。

## 一、目标

在保留全部历史证据与回退能力的前提下，把项目入口从"patched 补丁叠加 + 旧 wrapper"收敛到
"迁移源码树 + manifest 黑盒载荷 + 新构建器"，并让文档/检查脚本如实反映当前状态。

## 二、永久保留清单

**历史内容一律永久保留，禁止物理删除**；对旧 wrapper 而言，第二步允许**变更工作路径**（移入
`legacy/`），但不允许删除、丢失历史或移除 p27/17/8 回退入口。逐项说明：

| 项 | 保留要求 |
| --- | --- |
| `patches/` 全部 patch 文件与 `docs/patches/` 验收记录 | 溯源与事故证据；**不得移动或删除** |
| `debs/…patched-27.deb`（SHA `f3841597…`）、`patched-17.deb`、`patched-8.deb` | 回退链 `4.0.0-i1 → p27 → … → p17 → p8` 的物理载体；**不得移动或删除**，回退入口永久可用 |
| p17–p27 全部 git tag（含 p27） | 可复现回退点；**永久保留，不得移动或删除** |
| `scripts/build-deepin-coherent.sh` | 旧构建器/oracle；check-docs 版本护栏依赖；**继续保留在 `scripts/`，第二步也不移动** |
| `build-patchedNN-*.sh` wrapper 与历史安装/卸载/恢复入口（`install-patched17-and-check.sh`、`install-patched8-and-check.sh`、`uninstall-patched*.sh`） | **历史内容永久保留**；第二步允许改变工作路径移入 `legacy/`（或保持原路径仅标记 deprecated）；禁止物理删除或丢失任何历史 |
| Deepin 202504 原 deb | 新构建器的唯一载荷基线；永久保留 |

## 三、第一步（现在执行，已完成/进行中）：标记 deprecated，只改文档与检查脚本

不移动、不删除任何文件。变更范围：

1. `scripts/README.md`：旧构建器与 patched wrapper 生命周期标记 `legacy`（保留、禁止作为新
   工作入口）；当前入口 = `build-innogpu-driver.sh`（新架构）。
2. `docs/user/new-device-install.md`：主安装路径 = `4.0.0-i1`（新构建器产出 + apt 安装）；
   `patched-17` 降级为保守回退（保留，不再作为默认入口）。
3. `docs/user/recovery.md`：首选回退 = `4.0.0-i1 → patched-27`（`apt install
   --allow-downgrades`）；历史链 `p27 → … → p8` 保留为深层回退。
4. `docs/project/dependencies.md`：`patched-27` 由"当前运行包"改为"保留的回退基线"；
   新增 `4.0.0-i1` 行（新架构、可复现 SHA）。
5. `docs/planning/history.md`：2026-08-20 条目注明当时运行态；新增 2026-08-21 条目（Phase 4
   完成、推进 4.0.0-i1、合并 main）。
6. `scripts/check-docs.sh`：保留全部旧构建器/版本护栏（不删除）；仅同步因文档当前态变化而需要
   匹配的 require_text；为历史 wrapper 护栏段补充"legacy retention"注释。

**第一步完成判据**：check-docs.sh PASS、git diff --check 通过、工作区干净；无文件移动/删除；
设备与运行态不变。

## 四、第二步（暂不执行，一个发布周期后评估）：移入 legacy/

触发条件（全部满足才评估）：

1. 至少一个完整发布周期（含 release 附件发布与跨硬件/电源矩阵中的关键项）；
2. **新设备 clone 安装验证**：全新 Debian Trixie 设备按更新后的 `new-device-install.md`
   从 `4.0.0-i1` 完成安装、重启、验收；
3. **恢复流程验证**：按更新后的 `recovery.md` 完成 `4.0.0-i1 → patched-27` 回退演练；
4. 检查脚本确认：移除旧 wrapper 后 check-docs 仍 PASS（护栏改写为"已退役"断言而非删除断言）。

第二步动作（仅评估后执行，需监督批准）：

- 旧 wrapper（`build-patchedNN-*.sh`、历史安装/卸载入口）移入 `legacy/` 或标记 deprecated；
- `build-deepin-coherent.sh` 保留在 `scripts/`（check-docs 护栏依赖），生命周期标记 legacy；
- `patches/`、p27/17/8 deb、全部历史 tag 继续永久保留；
- 文档同步更新并复跑 check-docs。

## 五、禁止事项（两步都适用）

- **不得物理删除或丢失历史**：`patches/`、历史 deb、历史 tag、旧 wrapper 的历史内容一律保留；
- 第二步只允许**变更旧 wrapper 的工作路径**（移入 `legacy/`）或保持原路径标记 deprecated；
  `patches/`、p27/17/8 deb 与全部历史 tag **不得移动或删除**；
- 不得在第二步条件未满足时把 `build-deepin-coherent.sh` 移出 `scripts/`（check-docs 护栏依赖）；
- 不得用新架构 deb 覆盖历史版本号（`4.0.0-iN` 与 `3.3.3.42-patched-N` 序列并行保留）；
- 不得宣称 Phase 5 完成而设备/文档仍停留在旧状态。

## 参考

- 监督指南 `migration-supervision.md` §四 阶段 5 门槛（监督分支 @ bd76e91，只读）。
- [source-tree-migration.md](source-tree-migration.md)、[phase4-device-validation.md](phase4-device-validation.md)。
