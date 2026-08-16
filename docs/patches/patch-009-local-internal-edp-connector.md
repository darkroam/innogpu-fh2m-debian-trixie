# Patch 009：本机内置面板的 eDP connector 语义修正

## 目的

本机 `KaiTian X7 G1e` 的内置面板实际连接在 Innogpu 的 DP0 接口上，但当前硬件模式查询失败时，
驱动会把该接口回退为 `DRM_MODE_CONNECTOR_DisplayPort`。因此 DRM sysfs 暴露为 `DP-1`，
`systemd-logind` 会把内置面板误判为外接显示器，进而使 `Docked=true`。

这会破坏本应由 Debian logind 提供的策略：电池供电、无外屏合盖时挂起；接入外屏或外部电源时
继续运行。

## 实现

- 代码：`patches/009-local-internal-edp-connector.patch`。
- 构建开关：`APPLY_LOCAL_INTERNAL_EDP=1`。
- 固定条件：`s_dpu_match=141`，对应本机 ACPI/DPU 映射；不改变其他设备或其他 connector。
- 当 DP0 的 hwinfo/output-mode 查询失败时，选择 `dp_output_mode[9]`，即 `DRM_MODE_CONNECTOR_eDP`。
- 同时将接口元数据标记为 eDP，避免接口信息和实际 connector 类型不一致。
- HDMI、真实外接 DP、VGA 和其他 DPU 匹配值保持原有逻辑。

该补丁不修改 xdisplay，不写入 logind 配置，也不在驱动中实现挂起策略。它只修正驱动提供给
内核和系统电源管理的硬件事实。

## 构建与状态

patched-22 构建入口为：

```sh
scripts/build-patched22-local-lid.sh
```

候选包从 `innogpu-fh2m_20250421190503-debug_amd64.deb` 重新构建，版本为
`3.3.3.42-patched-22`，并已通过 `check-release-package.sh`。该包已在当前设备安装并重启验证；
当前设备的完整图形验收基线仍参考 patched-21，p22 的电源/合盖矩阵尚未全部完成。

从候选包解包后的 DKMS 源码在当前 `6.12.96+deb13-amd64` headers 上完成模块编译检查；编译仅有
Deepin 原有的兼容性 warning，没有 patch-009 引入的错误。候选 deb SHA-256 为
`aae8f966af7c5737037869a4e6ee5d081fd07d386dec67e0e799746ff6386ae9`。

## 重启后已完成的运行验证

2026-08-16 在当前设备读取到：包版本为 `3.3.3.42-patched-22`，内核为
`6.12.96+deb13-amd64`，PVR Driver/Firmware 均为 `OK`，DRM 内置面板为
`card0-eDP-1=connected`，外接 HDMI 均为 disconnected；当前开盖桌面状态为
`Docked=false`、`LidClosed=false`、`OnExternalPower=true`。RandR 把 `eDP-1` 设为
primary，xdisplay 报告 `INTERNAL_ONLY`、`health=ready`，未发现显示布局回归。
`scripts/verify-install-status.sh --require-reboot 3.3.3.42-patched-22` 返回
`RESULT: PASS_INSTALL_STATUS`。

以上只证明 connector 分类和开盖桌面路径，不等同于完整电源策略验收。以下矩阵仍需在电池和实际
外屏热插拔条件下逐项完成。

## 预期运行结果

logind 配置保持：

```ini
HandleLidSwitch=suspend
HandleLidSwitchExternalPower=ignore
HandleLidSwitchDocked=ignore
```

| 外部电源 | 外屏 | 合盖行为 |
| --- | --- | --- |
| 否 | 否 | 挂起 |
| 否 | 是 | 继续运行 |
| 是 | 否 | 继续运行 |
| 是 | 是 | 继续运行 |
| 外屏已连接、电池合盖后拔掉外屏 | 否 | 重新评估后挂起 |

真正接入外屏时，`Docked` 保持标准 logind 语义；本补丁只消除内置面板造成的假外屏。

## 验证门槛

安装前后必须分别记录：

```sh
for prop in Docked OnExternalPower LidClosed; do
  printf '%s=' "$prop"
  busctl get-property org.freedesktop.login1 /org/freedesktop/login1 \
    org.freedesktop.login1.Manager "$prop"
done
for f in /sys/class/drm/card0-*/status; do printf '%s=' "$f"; cat "$f"; done
xrandr --current
```

实机至少验证：

1. 无外屏、电池、开盖后合盖，系统挂起；
2. 外屏、电池、合盖，系统继续运行；
3. 保持合盖和电池供电，拔掉外屏，系统在拓扑重新评估后挂起；
4. 接入电源后无外屏和有外屏两种情况合盖均继续运行；
5. 开盖、fbterm、xdisplay 和外屏热插拔不回归。

若第 3 项在当前 systemd 版本中不自动触发，记录 `Docked`/DRM 变化和 logind 日志后，另行设计
仅针对 DRM 热插拔的触发接入，不在本补丁中加入第二套合盖处理器。

## 回退

patched-22 未通过任一实机门槛时，恢复 patched-21；不要从 patched-20 或更早包中拼接单个
驱动/用户态文件。回退后应记录 DRM connector 名称恢复为 `DP-1`，并明确当前假外屏问题仍存在。
