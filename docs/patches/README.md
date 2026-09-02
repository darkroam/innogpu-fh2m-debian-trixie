# 阶段补丁

每个补丁文件只描述一个可独立审查的变更。源码 diff 位于仓库 `patches/`；stage-000 因目标是厂商
预编译对象，使用 `tools/patch-gpupll-object.py` 执行严格字节契约。本目录记录各阶段的目的、应用
条件、验证证据和回退边界。构建入口只允许以 Deepin 202504 完整原包为载荷基线。

**分类说明（`patches/` 目录内两类内容）**：

- `patches/*.patch`（14 个源码 diff：001–009、023–027；另有 stage-000 确定性工具）——其中 13 个
  历史补丁已在源码树迁移时转为 `drivers/` 内的提交，不再重复叠加；新增 patch-024 是
  `4.0.1-i1` 的独立实验修复，由新架构构建器确定性应用，但 s2idle 可见恢复验收失败。本表保留 provenance、事故证据与
  legacy 回退包复现依据。
- `components/picom/`、`components/fbterm/`——**当前维护的第三方组件补丁与配置**（2026-08-21 由
  `patches/picom/`、`patches/fbterm/`、`config/` 迁入，历史内容保留）：补丁由
  `scripts/build-patched-picom.sh`、`scripts/build-patched-fbterm.sh` 在构建对应组件时应用，
  与驱动包构建无关；`components/picom/picom.conf` 是项目维护的配置模板，由
  `scripts/install-picom-user.sh` 作为默认配置源安装。

## 历史内核和驱动补丁

| 阶段 | 代码补丁 | 构建开关/入口 | 状态 |
| --- | --- | --- | --- |
| 000 | [skip-first-gpupll](patch-000-skip-first-gpupll.md) | 始终应用 | patched-19 至 p27 启用；4.0.0-i1 由确定性工具继续应用 |
| 001 | [kernel-6.12](patch-001-kernel-6.12.md) | 始终应用 | patched-19 至 p27 启用；4.0.0-i1 源码树已包含，p24+ 实机适配 6.12.101+ |
| 002 | [dp-fbdev-fallback](patch-002-dp-fbdev-fallback.md) | `APPLY_DP_FBCON_FALLBACK=1` | patched-19 至 p27 启用；4.0.0-i1 源码树已包含 |
| 003 | [panel-backlight-fallback](patch-003-panel-backlight-fallback.md) | `APPLY_PANEL_BACKLIGHT_FALLBACK=1` | 历史验证；当前关闭 |
| 004 | [panel-platform-fallback](patch-004-panel-platform-fallback.md) | `APPLY_PANEL_PLATFORM_FALLBACK=1` | 历史验证；当前关闭 |
| 005 | [backlight-initial-enable](patch-005-backlight-initial-enable.md) | `APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=1` | 历史验证；当前关闭 |
| 006 | [local-connector-acpi-map](patch-006-local-connector-acpi-map.md) | `APPLY_LOCAL_CONNECTOR_ACPI_MAP=1` | patched-19 至 p27 启用；p21 已在当前设备运行验收，后续版本继承；4.0.0-i1 源码树已包含 |
| 007 | [fbdev-io-mmap](patch-007-fbdev-io-mmap.md) | `APPLY_FBDEV_IO_MMAP=1` | patched-19 至 p27 启用并实机通过；4.0.0-i1 源码树已包含 |
| 008 | [pvr-init-diagnostic](patch-008-pvr-init-diagnostic.md) | `APPLY_PVR_INIT_DIAGNOSTIC=1` | 仅 patched-20 诊断启用 |
| 009 | [local-internal-edp-connector](patch-009-local-internal-edp-connector.md) | `APPLY_LOCAL_INTERNAL_EDP=1` | patched-22 至 p27 继承；connector/桌面烟测通过，4.0.0-i1 源码树已包含；电源与合盖矩阵待完成 |
| 023 | [invisible-read-no-writeback](patch-023-invisible-read-no-writeback.md) | `APPLY_INVISIBLE_READ_NO_WRITEBACK=1` | patched-23 至 p27 继承并实机通过；4.0.0-i1 源码树已包含；Clash 启动态 A/B 已完成 |
| 024 | [suspend-resume](024-suspend-resume.md) | 新架构 `4.0.1-i1` 固定应用；legacy `APPLY_SUSPEND_RESUME_FIX=1` | 构建/启动门禁通过；真实 s2idle 无 PowerLock 错误但外屏红屏，候选失败并回退；deep 未测试 |
| 025 | [dma-resv-usage-rw](patch-025-dma-resv-usage-rw.md) | `APPLY_DMA_RESV_USAGE_FIX=1` | patched-25 至 p27 继承并实机通过；4.0.0-i1 源码树已包含 |
| 026 | [inactive-crtc-vblank-guard](patch-026-inactive-crtc-vblank-guard.md) | `APPLY_INACTIVE_CRTC_VBLANK_GUARD=1` | patched-26/p27 实机通过；4.0.0-i1 源码树已包含；活动/未活动 CRTC 回归通过 |
| 027 | [foreign-dmabuf-lifecycle](patch-027-foreign-dmabuf-lifecycle.md) | `APPLY_FOREIGN_DMABUF_LIFECYCLE_FIX=1` | patched-27 实机验证安装/HWGL/DRI3 自导入；4.0.0-i1 源码树已包含；foreign/跨设备路径仍未实机触发 |

patched-24 不增加新的设备行为补丁；它沿用 patched-23 的补丁集合，并把 patch-001 的
`6.12.101+` PCI API 兼容修复打包进去，供新内核 headers 的 DKMS 自动构建使用。

## 用户态补丁

| 阶段 | 代码补丁 | 状态 |
| --- | --- | --- |
| Picom-001 | [explicit-uniform-location](picom/patch-picom-001-explicit-uniform-location.md) | 实机通过 |
| fbterm-001 | [configurable-redraw-scrolling](../incidents/fbterm-ypan-rendering.md) | 真实 VT 验证通过 |

## 构建顺序

**当前新架构（已回退并运行 4.0.0-i1；4.0.1-i1 为失败候选）**：

```text
Deepin 202504 原 deb
  -> scripts/build-innogpu-driver.sh（drivers/ 源码树 + patch-024 + manifest 黑盒 + 确定性变换）
  -> 离线 DKMS 编译 + 完整包组装（仅应用版本已审核的 patch-024）
  -> scripts/check-release-package.sh
```

**历史 patched 包（legacy，保留）**：

```text
Deepin 202504 原 deb
  -> 新版本号（必须 >20）和已审查补丁开关
  -> scripts/build-deepin-coherent.sh
       -> stage-000 GPU PLL 对象变换和 patch-001 始终应用
       -> wrapper 显式选择 patch-002 至 patch-007
       -> scripts/check-release-package.sh
  -> 打包、DKMS、固件完整性检查
  -> 隔离 Xorg/GLX
  -> 重启后 PVR/DRM/fbdev/fbterm
```

不要把某一阶段的 `.so`、固件或 maintainer script 从另一个 patched 包复制到当前构建中。
历史 patched-8、patched-17、patched-18、patched-19 只可作为证据或回退物，不能作为后续载荷父版本。
表中的“历史验证”不等于当前候选启用；复现具体包时以对应 wrapper 的环境变量为准。

patched-19/20 的固定 wrapper 已改为拒绝执行，因为当前源码的辅助载荷边界与原历史 deb 不同；继续
使用相同版本号会制造“版本相同、包内容不同”的不可审计产物。表中的 p19/20 开关集合只记录当时
实际启用的驱动补丁，不表示当前可以重建同名包。

## 版本候选

- [patched-21：所有权收敛后的首个 release candidate](patched-21-release-candidate.md)：固定启用
  stage-000、patch-001/002/006/007，关闭 patch-003/004/005/008；分开记录构建、包边界与运行
  验收。p21 已完成当前设备运行验收，仍不能继承 p20 的包或运行证据，也尚未完成跨硬件发布。
- patched-22：`scripts/build-patched22-local-lid.sh` 固定启用 patch-009，已从 Deepin 202504
  原包构建、通过包边界检查并在当前设备重启；它只修正本机内置 DP0/eDP 语义，电源与合盖实机矩阵仍待完成。
- patched-23：`scripts/build-patched23-invisible-read-fix.sh` 在 p22 开关集合上只增加 patch-023，修复
  invisible READ mapping 释放时的无意义回写；历史上已安装、重启并完成基础图形与 Clash 启动态 A/B，
  当前只作 provenance/回退链证据。
- patched-24：`scripts/build-patched24-kernel-612101.sh` 沿用 p23 全部开关，增加 Debian
  `6.12.101` 及以后 headers 的 `pci_resize_resource(..., exclude_bars)` 兼容分支；构建和安装前
  必须重新执行对应内核的 DKMS 编译验证；2026-08-18 已重启并确认 p24、DKMS、Driver/Firmware
  和 DRM/fbdev 正常。
- patched-25：`scripts/build-patched25-dma-resv-fix.sh` 增加 patch-025（CPU_PREP 的 dma_resv
  usage 语义修复）；已实机验证并合并、打 tag。
- patched-26：`scripts/build-patched26-vblank-guard.sh` 增加 patch-026（未活动 CRTC vblank 守卫）；
  已实机验证并合并、打 tag。
- patched-27：`scripts/build-patched27-foreign-dmabuf.sh` 增加 patch-027（foreign DMA-BUF 生命周期
  修复）；已实机验证并合并、打 tag。
- patched-28：`scripts/build-patched28-suspend-resume.sh` 继承 p27 并增加 patch-024（resume 早期
  devfreq 电源状态门禁）；补丁编号 024 是空缺回填，包版本不复用历史 patched-24；仅作 legacy
  对照。新架构 `4.0.1-i1` 的 s2idle 可见恢复验收已失败，不再作为可安装候选。
- p25/26/27 的 deb 均为可复现构建（[release 审阅](../planning/release-review-2026-08-20.md) 修复
  目录 mtime 后重建），SHA 见 [debs/README.md](../../debs/README.md)。
