# 当前状态与问题清单

最后更新：2026-08-14

本文件是项目当前运行状态的唯一摘要。历史过程、补丁细节和故障推导分别见
[阶段补丁](../patches/README.md) 与 [事故和经验](../incidents/README.md)。

## 当前基线

| 项目 | 当前结论 | 证据 |
| --- | --- | --- |
| 驱动包 | `3.3.3.42-patched-20` 已安装并重启成功 | `~/p20-kernel.log`、`dpkg -l` |
| 源码/用户态基线 | Deepin 202504 完整原包，不混用历史 patched 包 | `scripts/build-deepin-coherent.sh` |
| 固件 | `fh2m.fw`、`fh2m.sh` 均成功加载 | `~/p20-kernel.log` |
| PVR services | `state_before=2 -> ret=0 -> state_after=3 (ACTIVE)` | `~/p20-kernel.log` |
| DRM/fbdev | `card0`、`renderD128`、`fb0` 可用，fbdev mmap 成功 | patched-20 运行日志、fbterm trace |
| Xorg/GLX | Xorg、`xdpyinfo`、`glxinfo` 全部通过；硬件加速启用 | `baselines/latest-current-xorg-hwgl-test/result.txt` |
| 真实 VT | `fbterm rc=0`，可绘制并正常退出 | `~/fbterm-20p-result.txt` |
| 显示管理 | watcher、RandR 布局、状态和 fixture 已吸纳 | `tests/xdisplay/`、`docs/project/display-management.md` |
| Picom | Innogpu GLX 能力探测补丁已验证，配置独立于驱动包 | `patches/picom/`、`docs/project/compositor-management.md` |
| 音频 | HDA 内置喇叭绑定和 PipeWire 恢复已验证 | `docs/project/audio-management.md` |

## 已解决问题

| 问题 | 修复/结论 | 阶段 |
| --- | --- | --- |
| Debian 6.12 与厂商内核接口不兼容 | 通过兼容补丁适配 DKMS 构建 | `patch-001` |
| DP/面板在启动阶段无安全 fallback | 增加 DP fbcon、背光和平台回退路径 | `patch-002` 至 `patch-005` |
| 本机 connector 与 ACPI 映射差异 | 增加明确的设备映射钩子，不污染通用布局逻辑 | `patch-006` |
| patched-17 的 `/dev/fb0 mmap()` 返回 `ENODEV` | `fb_mmap = fb_io_mmap`，patched-20 实机验证成功 | `patch-007` |
| patched-18 用户态 ABI 混配 | 禁止从历史 deb 拼接用户态，统一以 Deepin 202504 原包重建 | 事故记录 |
| patched-18 缺少 shader 固件导致 PVR BAD | 保留完整 `fh2m.fw/fh2m.sh/fh2c.fw/fh2c.sh` | `patch-008` 及事故记录 |
| Picom 未声明 `GL_ARB_explicit_uniform_location` 而提前退出 | 运行时编译最小 shader 验证能力，成功后继续 | Picom patch |
| 显示布局脚本隐式依赖单外屏 | 已吸纳状态、外屏列表、链式布局和自定义布局 | 显示管理阶段 |

## 当前未解决或需要后续处理

1. `patch-008` 的 PVR 诊断会对每次 services ioctl 写日志；正式长期运行包应移除或限速，不能把
   当前诊断日志量当作稳定配置。
2. `hwinfo_g0m.bin` 仍缺失，但本次不阻止 PVR 进入 `ACTIVE`；是否需要该固件由后续硬件能力需求
   决定，不能仅凭缺失日志推断为故障。
3. 普通用户运行 `fbterm` 时不能修改内核键盘表，内置滚屏和切换 VT 快捷键不可用；这不是
   framebuffer 映射故障，不应直接授予全局特权。
4. patched-17 安装脚本仍是回退路径，尚未抽象出通用的 patched-20 安装器；后续发布前应建立
   明确的版本参数化安装流程。
5. 不同扩展坞、三块以上外屏、无盖桌面和多型号硬件的实机矩阵仍不完整。
6. 单设备 `xdisplay-device.local` 适配器和 manual marker 仍挂起，见
   [挂起项](../planning/suspended.md)。

## 证据保留规则

- Git 只提交精简的 `result.txt`、摘要和文档，不提交原始 Xorg、GLX、EDID 或 trace 日志。
- 原始日志保留在本机主目录，文件名必须包含版本和日期，例如 `p20-kernel.log`。
- 每次新版本验收必须同时记录：包版本、完整载荷来源、固件加载、PVR 状态、Xorg/GLX 和真实 VT。

## 发布判断

当前 patched-20 是“本机已验证的诊断候选”，不是长期发布基线。下一版本必须从 Deepin 202504
完整原包重新构建，先移除或限速诊断日志，再重复本文件列出的全部门槛。`patched-17` 保留为回退点，
`patched-8` 仅保留为历史回滚物。
