# 当前状态与问题清单

最后更新：2026-08-14

本文件是项目当前运行状态的唯一摘要。历史过程、补丁细节和故障推导分别见
[阶段补丁](../patches/README.md) 与 [事故和经验](../incidents/README.md)。

## 当前基线

| 项目 | 当前结论 | 证据 |
| --- | --- | --- |
| 驱动包 | `3.3.3.42-patched-20` 已安装并重启成功 | [`patched-20` 验收](../incidents/patched-20-runtime.md) |
| 包载荷边界 | 已验收 p20 deb 生成于 xdisplay 所有权收敛前，含旧引擎/实验辅助文件，不可发布或同版本重建 | [`patched-20` 载荷审计](../incidents/patched-20-legacy-helper-payload.md) |
| 下一候选 | patched-21 已完成两次一致构建和离线包审计，SHA-256 为 `15c1fab4...1384cc`；尚未安装或运行验收 | [`patched-21` 证据](../patches/patched-21-release-candidate.md) |
| 源码/用户态基线 | Deepin 202504 完整原包，不混用历史 patched 包 | `scripts/build-deepin-coherent.sh` |
| 固件 | `fh2m.fw`、`fh2m.sh` 均成功加载 | [`patched-20` 验收](../incidents/patched-20-runtime.md) |
| PVR services | `state_before=2 -> ret=0 -> state_after=3 (ACTIVE)` | [`patched-20` 验收](../incidents/patched-20-runtime.md) |
| DRM/fbdev | `card0`、`renderD128`、`fb0` 可用，fbdev mmap 成功 | patched-20 运行日志、fbterm trace |
| Xorg/GLX | Xorg、`xdpyinfo`、`glxinfo` 全部通过；硬件加速启用 | `baselines/latest-current-xorg-hwgl-test/result.txt` |
| 真实 VT | `fbterm rc=0`，可绘制并正常退出 | [`patched-20` 验收](../incidents/patched-20-runtime.md) |
| 显示管理 | dotconfig 维护 xdisplay 2.0.0；本项目只维护设备钩子和会话接入 | [`display-management.md`](display-management.md) |
| Picom | Innogpu GLX 能力探测补丁已验证，配置独立于驱动包 | `patches/picom/`、`docs/project/compositor-management.md` |
| 音频 | HDA 内置喇叭绑定和 PipeWire 恢复已验证 | `docs/project/audio-management.md` |

## 已解决问题

| 问题 | 修复/结论 | 阶段 |
| --- | --- | --- |
| Debian 6.12 与厂商内核接口不兼容 | 通过兼容补丁适配 DKMS 构建 | `patch-001` |
| DP 输出在启动阶段无安全 fallback | 当前候选启用 DP fbcon fallback | `patch-002` |
| 面板背光/平台注册和初始 enable 试验 | 历史补丁已保留，当前 patched-19/20 未启用 | `patch-003` 至 `patch-005` |
| 本机 connector 与 ACPI 映射差异 | 增加明确的设备映射钩子，不污染通用布局逻辑 | `patch-006` |
| patched-17 的 `/dev/fb0 mmap()` 返回 `ENODEV` | `fb_mmap = fb_io_mmap`，patched-20 实机验证成功 | `patch-007` |
| patched-18 用户态 ABI 混配 | 禁止从历史 deb 拼接用户态，统一以 Deepin 202504 原包重建 | 事故记录 |
| patched-18 缺少 shader 固件导致 PVR BAD | 从 Deepin 原包保留完整 `fh2m.fw/fh2m.sh/fh2c.fw/fh2c.sh` | coherent 构建及事故记录 |
| Picom 未声明 `GL_ARB_explicit_uniform_location` 而提前退出 | 运行时编译最小 shader 验证能力，成功后继续 | Picom patch |
| 显示引擎曾在本仓库形成重复副本 | 明确由 dotconfig 单独维护；本项目只注入 Innogpu 设备契约 | 显示集成阶段 |

## 当前未解决或需要后续处理

1. `patch-008` 的 PVR 诊断会对每次 services ioctl 写日志；正式长期运行包应移除或限速，不能把
   当前诊断日志量当作稳定配置。
2. `hwinfo_g0m.bin` 仍缺失，但本次不阻止 PVR 进入 `ACTIVE`；是否需要该固件由后续硬件能力需求
   决定，不能仅凭缺失日志推断为故障。
3. 普通用户运行 `fbterm` 时不能修改内核键盘表，内置滚屏和切换 VT 快捷键不可用；这不是
   framebuffer 映射故障，不应直接授予全局特权。
4. 当前源码的包辅助文件已完成所有权收敛，patched-21 也已完成离线构建与包审计；但尚未安装、
   重启或运行验收，不能作为已发布基线或替代当前 p20。
5. 已安装 p20 的 `/usr/share/innogpu-fh2m-trixie/` 仍包含旧显示安装器；在升级到新包前不得调用包内
   `innogpu-prepare-soft-xorg-dwm` 等会间接执行旧安装器的入口，应使用当前仓库脚本。
6. 不同扩展坞、三块以上外屏、无盖桌面和多型号硬件的实机矩阵仍不完整。
7. xdisplay 的适配器、状态机、配置和自定义布局由 dotconfig 独立演进；本项目只需持续验证
   `XDISPLAY_INTERNAL_OUTPUTS`、`XDISPLAY_RESTORE_COMMAND` 和会话接入仍兼容。
8. patched-21 的实际 deb 哈希和离线包审计已记录；后续实机门槛完成前，仍不能标记为可发布或
   替代当前 p20。

## 证据保留规则

- Git 只提交精简的 `result.txt`、摘要和文档，不提交原始 Xorg、GLX、EDID 或 trace 日志。
- 原始日志保留在本机主目录，文件名必须包含版本和日期，例如 `p20-kernel.log`。
- 每次新版本验收必须同时记录：包版本、完整载荷来源、固件加载、PVR 状态、Xorg/GLX 和真实 VT。

## 发布判断

当前 patched-20 是“本机已验证的历史诊断候选”，不是可发布基线。其运行结论不因辅助载荷审计而
失效，但原 deb 禁止推广，当前源码也禁止复用 p20 版本号。下一版本必须从 Deepin 202504 完整原包
重新构建，先移除或限速诊断日志，通过包边界审计，再重复本文件列出的全部运行门槛。
`patched-17` 保留为回退点，`patched-8` 仅保留为历史回滚物。

patched-21 已按上述要求从原包完成两次一致构建：关闭 `patch-008`，使用收敛后的辅助载荷，并通过
包边界审计。本轮只生成和读取了 deb；未安装、未重启和未做运行验收必须继续作为显式状态保留。
