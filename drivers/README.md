# drivers/ — FH2M 内核驱动源码树

## 来源与导入

- **来源**：Deepin 原包 `innogpu-fh2m_20250421190503-debug_amd64.deb` 内的
  `usr/src/innogpu-kernel-2.2/`（DKMS 源码树）。
- **导入**：2026-08-21，迁移阶段 1。源码树导入已完成，9 个启用补丁已转为转换提交
  （source/device-profile: patch-0XX，见 [patch-provenance.md](../docs/planning/patch-provenance.md)）。
- **导入提交**：见 git 历史（本目录首个提交）。
- **许可证**：文件头部声明为 Dual MIT/GPLv2（Imagination Technologies + Innosilicon 双版权）。

## 与迁移的关系

- 本目录是迁移后的**可维护源码树**（Git 跟踪），已包含 9 个启用补丁的转换提交；
  当前设备构建仍由旧流程（`third_party/` 解包 + `patches/` 补丁）承担，两者在迁移完成前并存，
  但 `drivers/` 源码与旧流程的 p27 生成树 parity 已通过。
- **排除项**（不进入本目录，由 binary-manifest.json 在阶段 2 创建后管理，当前文件尚不存在）：
  - 5 个预编译对象：`innogpu.o_shipped`、`innovpu.o_shipped`、`innosmmu.o_shipped`、
    `innodma.o_shipped`、`innosrvkm.o_shipped`（黑盒，进 `vendor/`）；
  - Deepin 包内混入的构建产物（`.o.cmd` 等，已清理）。
- **上游内容保留**：`innosrvkm/config_kernel.mk` 含厂商构建路径
  `/home/platform_headers/...`，属上游源码内容，保留以维持源码 parity，不代表本机路径。

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

## 修改规则（迁移完成前）

- 本目录 = Deepin 原始源码 + 9 个启用补丁的转换提交（source: patch-0XX / device-profile: patch-0XX，
  见 [patch-provenance.md](../docs/planning/patch-provenance.md)）；与 p27 生成源码树 parity 通过
  （可复现命令：`scripts/check-source-parity.sh`）。
- 后续修改直接以提交形式落库；黑盒对象（.o_shipped 等）不进入本目录，一律走 vendor/ + manifest。
