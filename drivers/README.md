# drivers/ — FH2M 内核驱动源码树

## 来源与导入

- **来源**：Deepin 原包 `innogpu-fh2m_20250421190503-debug_amd64.deb` 内的
  `usr/src/innogpu-kernel-2.2/`（DKMS 源码树）。
- **导入**：2026-08-21，迁移阶段 1。源码树导入已完成，9 个启用补丁已转为转换提交
  （source/device-profile: patch-0XX，见 [patch-provenance.md](../docs/planning/patch-provenance.md)）。
- **导入提交**：见 git 历史（本目录首个提交）。
- **许可证**：`drivers/` **不适用**根 [LICENSE](../LICENSE) 的 GPL-3.0-or-later；每个文件按
  [逐文件 inventory](../docs/project/source-license-inventory.tsv) 保留并沿用其自身声明。当前机械
  分类为 408 个 `MIT OR GPL-2.0-only`、2 个 `BSD-3-Clause OR LGPL-2.1-only`、3 个
  `Strictly Confidential` 和 70 个无许可实现/构建文件；`OR` 是对应文件的双许可选择，不覆盖其他
  路径。confidential 与无许可文件**排除出公开制品**（`driver-source` 制品非完整驱动，BLOCKED）；
  许可边界的唯一权威文档见 [licensing.md](../docs/project/licensing.md)。本文件是项目文档
  （原创层 GPL-3.0-or-later）。

## 与迁移的关系

- 本目录是迁移后的**可维护源码树**（Git 跟踪），已包含 9 个启用补丁的转换提交；当前
  `4.0.0-i1` 由 `scripts/build-innogpu-driver.sh` 直接使用本目录构建。旧 patch 叠加流程仅作为
  p27 历史 oracle/回退证据保留；`drivers/` 与 p27 生成树 parity 已通过。
- **排除项**（不进入本目录，由 `binary-manifest.json` 的 192 项清单管理）：
  - 5 个预编译对象：`innogpu.o_shipped`、`innovpu.o_shipped`、`innosmmu.o_shipped`、
    `innodma.o_shipped`、`innosrvkm.o_shipped`（黑盒，进 `vendor/`）；
  - Deepin 包内混入的构建产物（`.o.cmd` 等，已清理）。
- **上游内容保留**：`innosrvkm/config_kernel.mk` 含厂商绝对构建路径（文档记为
  `$UPSTREAM_BUILD_HOME/platform_headers/...`），属上游源码内容，保留以维持源码 parity，不代表本机路径。

## parity 状态

```text
source_import=PASS          (与 Deepin 源逐字一致，除设计排除项)
excluded_o_shipped=5        (innogpu/innovpu/innosmmu/innodma/innosrvkm)
excluded_build_artifacts=4  (.o.cmd)
working_tree_clean=PASS
```

## 目录

- `innosrvkm/`：PVR services 包装（`pvr_*.c`）+ DPU 显示（`innodpu_*/pdp0_*/g3_*`）
- `innogpu/`、`innovpu/`、`innosmmu/`、`innodma/`、`innopmbus/`、`innopower/`：Inno 自有硬件层（HAL/DMA/VPU/电源，含部分源码 + 依赖黑盒对象）
- `tools/`：厂商辅助工具源码
- 顶层 `Makefile`/`Kbuild`/`dkms.conf` 等：构建配置

## 修改规则

- 本目录 = Deepin 原始源码 + 9 个启用补丁的转换提交（source: patch-0XX / device-profile: patch-0XX，
  见 [patch-provenance.md](../docs/planning/patch-provenance.md)）；与 p27 生成源码树 parity 通过
  （可复现命令：`scripts/check-source-parity.sh`）。
- 后续修改直接以提交形式落库；黑盒对象（.o_shipped 等）不进入本目录，一律走 vendor/ + manifest。
