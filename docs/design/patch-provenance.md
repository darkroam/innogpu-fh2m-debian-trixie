# Patch Provenance 表（阶段 1）

## 状态

- 本表是迁移阶段 1 的交付物之一（与 drivers/ 源码树导入配套），记录全部 14 个 patch 的
  类别、启用状态、哈希、目标文件与转换计划。
- 阶段 1 已完成：导入源码树 + provenance 表 + **9 个启用补丁转源码提交**（000 保持工具形态）；
  source_tree_parity_against_p27=PASS（可复现命令：scripts/check-source-parity.sh）。
- 分类四类：source（源码提交）、binary-transform（确定性二进制变换）、device-profile（本机特例）、closed（关闭的历史试验）。
- 监督指南仅见监督分支 `migration/supervised-source-tree` @ `bd76e91` 中的
  `docs/planning/migration-supervision.md`，不在 `main`。

## 汇总

| 类别 | patch | 数量 |
| --- | --- | --- |
| source | 001, 002, 007, 023, 025, 026, 027 | 7 |
| binary-transform | 000 | 1 |
| device-profile | 006, 009 | 2 |
| closed | 003, 004, 005, 008 | 4 |

启用状态以 patched-27 开关集合为准：**启用 10 个**（000/001/002/006/007/009/023/025/026/027），关闭 4 个（003/004/005/008）。

## 明细

| # | 类别 | 启用 | patch SHA-256 | 目标文件 | 文档 | 转换计划 |
| --- | --- | --- | --- | --- | --- | --- |
| 000 | binary-transform | 是 | （工具）tools/patch-gpupll-object.py，SHA-256 e5f9ee94f55aed2507359d0708f2d88b5b3251582380f5792cab54892a35274a | innogpu/innogpu.o_shipped（单点字节） | [patch-000](../patches/patch-000-skip-first-gpupll.md) | 保留独立确定性工具；输入/输出 hash 入 manifest；不做成源码提交 |
| 001 | source | 是 | be5c8ae9e08f5a2979e939bd18d4f9cc35593c5333fe80b1fb7748ffdcf71ab5 | Kbuild；innogpu/innopmbus/innopower/innosmmu/innovpu 多文件；innosrvkm 11 文件（见下） | [patch-001](../patches/patch-001-kernel-6.12.md) | 拆分为源码提交；Kbuild 的 -Wno-error 改动归 build-metadata；转换提交 `0f9b736` |
| 002 | source | 是 | 1a12de65f201839232a99f542707f43240b175bbe80029926b4ed3ab180f7329 | innosrvkm/innodpu_connector.c、innodpu_dp.c | [patch-002](../patches/patch-002-dp-fbdev-fallback.md) | 源码提交；转换提交 `013536b` |
| 003 | closed | 否 | 8cd6b492b01e2c42c3eb6dfa8d7042bc2e5f57654159dc579a11a66d6a2c6f7b | innodpu_connector.c、innodpu_panel_backlight.c | [patch-003](../patches/patch-003-panel-backlight-fallback.md) | 仅历史记录，不导入当前行为 |
| 004 | closed | 否 | 330c3a06998400bb4235385ccaff24a27dc319df508f613eeaf7a80b42814513 | innodpu_panel_backlight.c、innodpu_panel_pwr.c | [patch-004](../patches/patch-004-panel-platform-fallback.md) | 仅历史记录 |
| 005 | closed | 否 | 9fee230ceb3347b05c107bdb4454fd8d19f3ee32d19f194c7f492526d6deec15 | innodpu_panel_backlight.c | [patch-005](../patches/patch-005-backlight-initial-enable.md) | 仅历史记录 |
| 006 | device-profile | 是 | 63a6569ccb13adfadc7d717cf0b1bc6a338ff7f0231429e6b9d266b0a07340b5 | innosrvkm/innodpu_connector.c | [patch-006](../patches/patch-006-local-connector-acpi-map.md) | 进入 device profile 层，保留明确边界；转换提交 `467ade1` |
| 007 | source | 是 | 1adb7a3744abb936d41e4c6e54f490f4535b3a2e344d0538aa9368a824337733 | innosrvkm/innodpu_drm_fb.c | [patch-007](../patches/patch-007-fbdev-io-mmap.md) | 源码提交；转换提交 `e187da5` |
| 008 | closed | 否 | 4cfd545afcfbac337e8a018cfaea4f079d28d0332be6cddfea167cb84bfb6394 | innosrvkm/innogpu_drm.c | [patch-008](../patches/patch-008-pvr-init-diagnostic.md) | 仅历史记录 |
| 009 | device-profile | 是 | e6b955fd3cbde69f211098108c2770fdce8d8eb052096ee20d03b9522e5bb26c | innosrvkm/innodpu_connector.c | [patch-009](../patches/patch-009-local-internal-edp-connector.md) | 进入 device profile 层；转换提交 `3c127e0` |
| 023 | source | 是 | ea35a852d3b0d2818cd1abfbf20c888eacaccdc87371fd9a20f9520d5ee01f63 | innosrvkm/innodpu_drm_gem.c、include/innodpu_drm_gem.h | [patch-023](../patches/patch-023-invisible-read-no-writeback.md) | 源码提交；转换提交 `4041bcb` |
| 025 | source | 是 | 05de1bdd503d3a83d82b7e0de44d74535c39d02ae153be4649c90e0c1ae0a027 | innosrvkm/innodpu_drm_gem.c、include/innodpu_compatibility.h | [patch-025](../patches/patch-025-dma-resv-usage-rw.md) | 源码提交；转换提交 `436ec99` |
| 026 | source | 是 | 864bc3d651250ed9b701d376f6408fe7f506b2fea5fe7129007034e95a80216b | innosrvkm/pdp0_crtc.c | [patch-026](../patches/patch-026-inactive-crtc-vblank-guard.md) | 源码提交；转换提交 `528370f` |
| 027 | source | 是 | ab2d1b418fa315fc0528c65e425ad37e16ecdde9ae81066869a487a5c6ce7ca5 | innosrvkm/innodpu_drm_gem.c | [patch-027](../patches/patch-027-foreign-dmabuf-lifecycle.md) | 源码提交；转换提交 `b2b30da` |

### patch-001 目标文件明细（多文件兼容补丁）

text 块：
Kbuild；innogpu/compat_kernel6.h, hal_power.c, inno_drm.c, inno_mm.c,
inno_pci.c, inno_task.c, inno_uuid.c；innopmbus/innopmbus_drv.c；
innopower/inno_devfreq_gov.c；innosmmu/innosmmu_drv.c；innovpu/innovpu_drv.c；
innosrvkm/gen_g3_ne_hdmi.c, pvr_fence_trace.h, rogue_trace_events.h,
innodpu_dp.c, innodpu_drm_modeset.c, innodpu_hdmi.c, innodpu_panel_backlight.c,
innodpu_vga.c, innogpu_drm.c, pvr_drm.c。

转换时需把 Kbuild 的 -Wno-error 豁免归入 build-metadata，其余为 6.12 兼容源码提交。

## 转换提交规则（已执行）

- commit message 保留原编号，如 source: patch-025 dma_resv usage semantics；✅ 已按此执行
- commit body 引用 docs/patches/patch-*.md；✅
- 记录：原 patch hash 到转换提交 hash 到行为变化；✅（哈希见上表明细）
- closed 补丁（003/004/005/008）不产生提交，仅保留历史文档；✅
- source_tree_parity_against_p27 门槛已通过（scripts/check-source-parity.sh，0 差异）。✅

**当前状态**：9 个启用补丁已转源码提交（哈希见上表），000 保持确定性工具；
parity 通过（scripts/check-source-parity.sh，0 差异）。原始 diff 保留在 patches/（迁移完成前不删除），
补丁编号即原始 diff 映射键。

## 与 drivers/ 的对应

drivers/ 以 Deepin 原始源码为基线导入，9 个启用补丁的改动已以转换提交形式落到对应文件
（目标文件与提交哈希见上表明细）。
