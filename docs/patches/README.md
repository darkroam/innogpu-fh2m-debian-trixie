# 阶段补丁

每个补丁文件只描述一个可独立审查的变更。代码补丁位于仓库 `patches/`，本目录记录其目的、
应用条件、验证证据和回退边界。构建入口只允许以 Deepin 202504 完整原包为载荷基线。

## 内核和驱动补丁

| 阶段 | 代码补丁 | 构建开关/入口 | 状态 |
| --- | --- | --- | --- |
| 001 | [kernel-6.12](patch-001-kernel-6.12.md) | 始终应用 | 已验证 |
| 002 | [dp-fbdev-fallback](patch-002-dp-fbdev-fallback.md) | `APPLY_DP_FBCON_FALLBACK=1` | 已验证 |
| 003 | [panel-backlight-fallback](patch-003-panel-backlight-fallback.md) | `APPLY_PANEL_BACKLIGHT_FALLBACK=1` | 已验证 |
| 004 | [panel-platform-fallback](patch-004-panel-platform-fallback.md) | `APPLY_PANEL_PLATFORM_FALLBACK=1` | 已验证 |
| 005 | [backlight-initial-enable](patch-005-backlight-initial-enable.md) | `APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=1` | 已验证 |
| 006 | [local-connector-acpi-map](patch-006-local-connector-acpi-map.md) | `APPLY_LOCAL_CONNECTOR_ACPI_MAP=1` | 本机特例 |
| 007 | [fbdev-io-mmap](patch-007-fbdev-io-mmap.md) | `APPLY_FBDEV_IO_MMAP=1` | 实机通过 |
| 008 | [pvr-init-diagnostic](patch-008-pvr-init-diagnostic.md) | `APPLY_PVR_INIT_DIAGNOSTIC=1` | 诊断候选 |

## 用户态补丁

| 阶段 | 代码补丁 | 状态 |
| --- | --- | --- |
| Picom-001 | [explicit-uniform-location](picom/patch-picom-001-explicit-uniform-location.md) | 实机通过 |

## 构建顺序

```text
Deepin 202504 原 deb
  -> scripts/build-deepin-coherent.sh
       -> patch-001 至 patch-007
       -> 可选 patch-008 诊断
  -> 打包、DKMS、固件完整性检查
  -> 隔离 Xorg/GLX
  -> 重启后 PVR/DRM/fbdev/fbterm
```

不要把某一阶段的 `.so`、固件或 maintainer script 从另一个 patched 包复制到当前构建中。
历史 patched-8、patched-17、patched-18、patched-19 只可作为证据或回退物，不能作为后续载荷父版本。
