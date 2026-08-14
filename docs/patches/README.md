# 阶段补丁

每个补丁文件只描述一个可独立审查的变更。源码 diff 位于仓库 `patches/`；stage-000 因目标是厂商
预编译对象，使用 `tools/patch-gpupll-object.py` 执行严格字节契约。本目录记录各阶段的目的、应用
条件、验证证据和回退边界。构建入口只允许以 Deepin 202504 完整原包为载荷基线。

## 内核和驱动补丁

| 阶段 | 代码补丁 | 构建开关/入口 | 状态 |
| --- | --- | --- | --- |
| 000 | [skip-first-gpupll](patch-000-skip-first-gpupll.md) | 始终应用 | patched-19/20 启用 |
| 001 | [kernel-6.12](patch-001-kernel-6.12.md) | 始终应用 | patched-19/20 启用 |
| 002 | [dp-fbdev-fallback](patch-002-dp-fbdev-fallback.md) | `APPLY_DP_FBCON_FALLBACK=1` | patched-19/20 启用 |
| 003 | [panel-backlight-fallback](patch-003-panel-backlight-fallback.md) | `APPLY_PANEL_BACKLIGHT_FALLBACK=1` | 历史验证；当前关闭 |
| 004 | [panel-platform-fallback](patch-004-panel-platform-fallback.md) | `APPLY_PANEL_PLATFORM_FALLBACK=1` | 历史验证；当前关闭 |
| 005 | [backlight-initial-enable](patch-005-backlight-initial-enable.md) | `APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=1` | 历史验证；当前关闭 |
| 006 | [local-connector-acpi-map](patch-006-local-connector-acpi-map.md) | `APPLY_LOCAL_CONNECTOR_ACPI_MAP=1` | patched-19/20 本机启用 |
| 007 | [fbdev-io-mmap](patch-007-fbdev-io-mmap.md) | `APPLY_FBDEV_IO_MMAP=1` | patched-19/20 启用，实机通过 |
| 008 | [pvr-init-diagnostic](patch-008-pvr-init-diagnostic.md) | `APPLY_PVR_INIT_DIAGNOSTIC=1` | 仅 patched-20 诊断启用 |

## 用户态补丁

| 阶段 | 代码补丁 | 状态 |
| --- | --- | --- |
| Picom-001 | [explicit-uniform-location](picom/patch-picom-001-explicit-uniform-location.md) | 实机通过 |

## 构建顺序

```text
Deepin 202504 原 deb
  -> scripts/build-deepin-coherent.sh
       -> stage-000 GPU PLL 对象变换和 patch-001 始终应用
       -> wrapper 显式选择 patch-002 至 patch-007
       -> patched-20 额外启用 patch-008 诊断
  -> 打包、DKMS、固件完整性检查
  -> 隔离 Xorg/GLX
  -> 重启后 PVR/DRM/fbdev/fbterm
```

不要把某一阶段的 `.so`、固件或 maintainer script 从另一个 patched 包复制到当前构建中。
历史 patched-8、patched-17、patched-18、patched-19 只可作为证据或回退物，不能作为后续载荷父版本。
表中的“历史验证”不等于当前候选启用；复现具体包时以对应 wrapper 的环境变量为准。
