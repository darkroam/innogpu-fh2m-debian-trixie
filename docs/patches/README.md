# 阶段补丁

每个补丁文件只描述一个可独立审查的变更。源码 diff 位于仓库 `patches/`；stage-000 因目标是厂商
预编译对象，使用 `tools/patch-gpupll-object.py` 执行严格字节契约。本目录记录各阶段的目的、应用
条件、验证证据和回退边界。构建入口只允许以 Deepin 202504 完整原包为载荷基线。

## 内核和驱动补丁

| 阶段 | 代码补丁 | 构建开关/入口 | 状态 |
| --- | --- | --- | --- |
| 000 | [skip-first-gpupll](patch-000-skip-first-gpupll.md) | 始终应用 | patched-19/20/21 启用 |
| 001 | [kernel-6.12](patch-001-kernel-6.12.md) | 始终应用 | patched-19/20/21 启用 |
| 002 | [dp-fbdev-fallback](patch-002-dp-fbdev-fallback.md) | `APPLY_DP_FBCON_FALLBACK=1` | patched-19/20/21 启用 |
| 003 | [panel-backlight-fallback](patch-003-panel-backlight-fallback.md) | `APPLY_PANEL_BACKLIGHT_FALLBACK=1` | 历史验证；当前关闭 |
| 004 | [panel-platform-fallback](patch-004-panel-platform-fallback.md) | `APPLY_PANEL_PLATFORM_FALLBACK=1` | 历史验证；当前关闭 |
| 005 | [backlight-initial-enable](patch-005-backlight-initial-enable.md) | `APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=1` | 历史验证；当前关闭 |
| 006 | [local-connector-acpi-map](patch-006-local-connector-acpi-map.md) | `APPLY_LOCAL_CONNECTOR_ACPI_MAP=1` | patched-19/20/21 启用；p21 已在当前设备运行验收 |
| 007 | [fbdev-io-mmap](patch-007-fbdev-io-mmap.md) | `APPLY_FBDEV_IO_MMAP=1` | patched-19/20/21 启用且实机通过；p21 真实 VT fbterm 已通过 |
| 008 | [pvr-init-diagnostic](patch-008-pvr-init-diagnostic.md) | `APPLY_PVR_INIT_DIAGNOSTIC=1` | 仅 patched-20 诊断启用 |
| 009 | [local-internal-edp-connector](patch-009-local-internal-edp-connector.md) | `APPLY_LOCAL_INTERNAL_EDP=1` | patched-22 已安装并重启；connector/桌面烟测通过，电源与合盖矩阵待完成 |
| 023 | [invisible-read-no-writeback](patch-023-invisible-read-no-writeback.md) | `APPLY_INVISIBLE_READ_NO_WRITEBACK=1` | patched-23 已安装、重启并完成驱动/桌面/最小探针验证；Clash 启动态 A/B 已完成 |

patched-24 不增加新的设备行为补丁；它沿用 patched-23 的补丁集合，并把 patch-001 的
`6.12.101+` PCI API 兼容修复打包进去，供新内核 headers 的 DKMS 自动构建使用。

## 用户态补丁

| 阶段 | 代码补丁 | 状态 |
| --- | --- | --- |
| Picom-001 | [explicit-uniform-location](picom/patch-picom-001-explicit-uniform-location.md) | 实机通过 |

## 构建顺序

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
  invisible READ mapping 释放时的无意义回写；deb 和 DKMS 离线编译已通过，当前未安装、不热切换。
- patched-24：`scripts/build-patched24-kernel-612101.sh` 沿用 p23 全部开关，增加 Debian
  `6.12.101` 及以后 headers 的 `pci_resize_resource(..., exclude_bars)` 兼容分支；构建和安装前
  必须重新执行对应内核的 DKMS 编译验证。
