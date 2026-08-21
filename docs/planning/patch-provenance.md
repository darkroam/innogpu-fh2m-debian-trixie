# Patch Provenance 表（阶段 1）

## 状态

- 本表是迁移阶段 1 的交付物之一（与 drivers/ 源码树导入配套），记录全部 14 个 patch 的
  类别、启用状态、哈希、目标文件与转换计划。
- 当前仅完成"导入源码树 + 建立 provenance"；**补丁转源码提交未执行**（待监督批准）。
- 分类四类：source（源码提交）、binary-transform（确定性二进制变换）、device-profile（本机特例）、closed（关闭的历史试验）。
- 监督指南见 docs/planning/migration-supervision.md（监督分支 migration/supervised-source-tree @ bd76e91）。

## 汇总

| 类别 | patch | 数量 |
| --- | --- | --- |
| source | 001, 002, 007, 023, 025, 026, 027 | 7 |
| binary-transform | 000 | 1 |
| device-profile | 006, 009 | 2 |
| closed | 003, 004, 005, 008 | 4 |

启用状态以 patched-27 开关集合为准：启用 9 个（000/001/002/006/007/009/023/025/026/027），关闭 4 个（003/004/005/008）。

## 明细

| # | 类别 | 启用 | patch SHA-256 | 目标文件 | 文档 | 转换计划 |
| --- | --- | --- | --- | --- | --- | --- |
| 000 | binary-transform | 是 | （工具）tools/patch-gpupll-object.py | innogpu/innogpu.o_shipped（单点字节） | [patch-000](../patches/patch-000-skip-first-gpupll.md) | 保留独立确定性工具；输入/输出 hash 入 manifest；不做成源码提交 |
| 001 | source | 是 | be5c8ae9...71ab5 | Kbuild；innogpu/innopmbus/innopower/innosmmu/innovpu 多文件；innosrvkm 11 文件（见下） | [patch-001](../patches/patch-001-kernel-6.12.md) | 拆分为源码提交；Kbuild 的 -Wno-error 改动归 build-metadata |
| 002 | source | 是 | 1a12de65...7329 | innosrvkm/innodpu_connector.c、innodpu_dp.c | [patch-002](../patches/patch-002-dp-fbdev-fallback.md) | 源码提交 |
| 003 | closed | 否 | 8cd6b492...c6f7b | innodpu_connector.c、innodpu_panel_backlight.c | [patch-003](../patches/patch-003-panel-backlight-fallback.md) | 仅历史记录，不导入当前行为 |
| 004 | closed | 否 | 330c3a06...4513 | innodpu_panel_backlight.c、innodpu_panel_pwr.c | [patch-004](../patches/patch-004-panel-platform-fallback.md) | 仅历史记录 |
| 005 | closed | 否 | 9fee230c...ec15 | innodpu_panel_backlight.c | [patch-005](../patches/patch-005-backlight-initial-enable.md) | 仅历史记录 |
| 006 | device-profile | 是 | 63a6569c...40b5 | innosrvkm/innodpu_connector.c | [patch-006](../patches/patch-006-local-connector-acpi-map.md) | 进入 device profile 层，保留明确边界 |
| 007 | source | 是 | 1adb7a37...7733 | innosrvkm/innodpu_drm_fb.c | [patch-007](../patches/patch-007-fbdev-io-mmap.md) | 源码提交 |
| 008 | closed | 否 | 4cfd545a...6394 | innosrvkm/innogpu_drm.c | [patch-008](../patches/patch-008-pvr-init-diagnostic.md) | 仅历史记录 |
| 009 | device-profile | 是 | e6b955fd...b26c | innosrvkm/innodpu_connector.c | [patch-009](../patches/patch-009-local-internal-edp-connector.md) | 进入 device profile 层 |
| 023 | source | 是 | ea35a852...01f63 | innosrvkm/innodpu_drm_gem.c、include/innodpu_drm_gem.h | [patch-023](../patches/patch-023-invisible-read-no-writeback.md) | 源码提交 |
| 025 | source | 是 | 05de1bdd...a027 | innosrvkm/innodpu_drm_gem.c、include/innodpu_compatibility.h | [patch-025](../patches/patch-025-dma-resv-usage-rw.md) | 源码提交 |
| 026 | source | 是 | 864bc3d6...216b | innosrvkm/pdp0_crtc.c | [patch-026](../patches/patch-026-inactive-crtc-vblank-guard.md) | 源码提交 |
| 027 | source | 是 | ab2d1b41...7ca5 | innosrvkm/innodpu_drm_gem.c | [patch-027](../patches/patch-027-foreign-dmabuf-lifecycle.md) | 源码提交 |

### patch-001 目标文件明细（多文件兼容补丁）

text 块：
Kbuild；innogpu/compat_kernel6.h, hal_power.c, inno_drm.c, inno_mm.c,
inno_pci.c, inno_task.c, inno_uuid.c；innopmbus/innopmbus_drv.c；
innopower/inno_devfreq_gov.c；innosmmu/innosmmu_drv.c；innovpu/innovpu_drv.c；
innosrvkm/gen_g3_ne_hdmi.c, pvr_fence_trace.h, rogue_trace_events.h,
innodpu_dp.c, innodpu_drm_modeset.c, innodpu_hdmi.c, innodpu_panel_backlight.c,
innodpu_vga.c, innogpu_drm.c, pvr_drm.c。

转换时需把 Kbuild 的 -Wno-error 豁免归入 build-metadata，其余为 6.12 兼容源码提交。

## 转换提交规则（待批准后执行）

- commit message 保留原编号，如 source: patch-025 dma_resv usage semantics；
- commit body 引用 docs/patches/patch-*.md；
- 记录：原 patch hash 到转换提交 hash 到行为变化；
- closed 补丁（003/004/005/008）不产生提交，仅保留历史文档；
- source_tree_parity_against_p27 门槛在转换后执行（drivers/ + 转换提交 与 p27 生成树对比）。

## 与 drivers/ 的对应

已导入 drivers/ 为 Deepin 原始源码（未打补丁）。上述 source/device-profile 类补丁的改动
将在转换阶段以提交形式落到 drivers/ 对应文件（目标文件已列于本表）。
