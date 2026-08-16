# 显示管理集成

## 当前结论

通用 X11 显示引擎由 dotconfig 仓库独立维护，本项目不复制 `xdisplay`、`displayselect`、共享库、
配置文件或引擎内部测试。Innogpu 项目只负责声明本设备与通用引擎之间的接入契约，并提供无法放入
通用引擎的固定 modeline 恢复钩子。

当前 dotconfig 侧的权威文件为：

```text
.local/bin/xdisplay
.local/bin/xdisplay.sh                  # 兼容包装
.local/bin/displayselect
.local/lib/xdisplay/
.local/share/docs/project/display-device-adapter.md
.local/share/test/display/xdisplay-adapter.sh
```

状态枚举、多外屏链式布局、配置系统、自定义布局、适配器灰度调用、锁、退避和 `--status` 均由
dotconfig 负责设计和验证。相关行为发生变化时，应在 dotconfig 修改代码、测试和文档；本项目只复核
下面的环境变量与设备钩子仍兼容。

## 所有权边界

| 所有者 | 文件/接口 | 职责 |
| --- | --- | --- |
| dotconfig | `xdisplay`、`xdisplay.sh`、`displayselect`、`~/.local/lib/xdisplay/` | 通用状态计算、布局、锁、配置、适配器和用户命令 |
| Innogpu | `scripts/restore-dp1-mode-x11.sh` | 为本设备恢复固定 DP-1 modeline |
| Innogpu | `scripts/xdisplay-session.sh` | 注入内屏候选和恢复命令，然后启动已安装的 xdisplay |
| Innogpu | `scripts/install-xdisplay-user.sh` | 安装设备钩子和带边界标记的会话接入，不安装或覆盖显示引擎 |
| Innogpu | `tests/xdisplay/run-install-tests.sh` | 只验证上述接入和所有权边界 |

本项目删除了曾经吸纳的 `scripts/xdisplay.sh`、`scripts/displayselect` 和引擎 fixture，避免两个仓库
分别维护同一状态机。历史吸纳过程仍可在
[`planning/display-integration.md`](../planning/display-integration.md) 中追溯，但不再代表当前所有权。

## 运行关系

```text
innogpu + Xorg
  -> RandR outputs/modes/geometry
  -> dotconfig: xdisplay watch
       -> lid + DRM connector + RandR
       -> 通用状态计算和布局收敛
       -> XDISPLAY_INTERNAL_OUTPUTS="eDP-1 DP-1"
       -> XDISPLAY_RESTORE_COMMAND=innogpu-restore-dp1-mode-x11

dotconfig: displayselect
  -> 与 watcher 共用 apply lock
  -> 手动布局及自定义布局保存/恢复
```

watcher 必须由活动 X11 会话启动并继承 `DISPLAY`、`XAUTHORITY` 和用户环境。不得另建 udev 直接调用
`xrandr` 的第二条链路，也不得在缺少完整图形会话环境的系统服务中启动另一个 watcher。

## 本设备契约

- patched-21 当前运行时仍观察到 DRM `DP-1`、`HDMI-A-1`、`HDMI-A-2`，对应 RandR
  `eDP-1`、`HDMI-1`、`HDMI-2`；其中 DRM `DP-1` 是本机内置面板的错误 connector 类型。
- patched-22 候选由 `patch-009` 在本机 `s_dpu_match=141` 且硬件模式查询失败时将该 DP0
  connector 修正为 `eDP`。该候选尚未安装前，不得把 eDP sysfs 名称写成当前事实。
- `XDISPLAY_INTERNAL_OUTPUTS="eDP-1 DP-1"` 是 dotconfig 兼容接入，p22 验证完成后仍可保留一段
  过渡周期；外屏始终由 RandR 动态发现。connector 类型和 DRM 映射以本项目阶段补丁文档为准，
  dotconfig 只消费其稳定的 X11 接入契约。
- `XDISPLAY_RESTORE_COMMAND=innogpu-restore-dp1-mode-x11` 是兼容恢复入口；失败时必须由通用引擎
  降级到 RandR preferred/首项逻辑并保留可见输出。
- `restore-dp1-mode-x11.sh` 中的 modeline 是本设备特例，不能进入 dotconfig 通用布局算法。
- logind 决定合盖是否挂起；xdisplay 只在 X11 会话仍运行时处理布局。当前目标配置为
  `HandleLidSwitch=suspend`、`HandleLidSwitchExternalPower=ignore`、
  `HandleLidSwitchDocked=ignore`：无外屏且电池合盖挂起，外屏或外部电源存在时继续运行。
- 内置面板被误报为 DRM `DP-1` 时会产生假 `Docked`，这是驱动 connector 语义问题，不由 xdisplay
  或适配器修补；权威修复记录见 [`patch-009`](../patches/patch-009-local-internal-edp-connector.md)。

### 合盖与外屏断开

标准 logind 会在合盖动作发生时重新计算 `Docked`。因此外屏接入、电池合盖时可以继续运行；外屏
拔出后必须确认当前 systemd 是否会主动重新评估已闭合的 lid。若本机不触发挂起，只能在记录
`Docked`、DRM hotplug 和 logind 日志后，另行增加窄范围的 DRM 热插拔触发接入；不得在 xdisplay
中加入第二套合盖处理器。

dotconfig 也支持可选的 `~/.config/x11/xdisplay-device.local` 适配器，但是否启用及其契约属于
dotconfig。Innogpu 当前默认接入仍使用上述两个兼容环境变量，不要求适配器存在。

## 安装行为

`scripts/install-xdisplay-user.sh` 的目标是“接入已有引擎”，而不是发布引擎：

1. 要求目标用户已经从 dotconfig 安装 `~/.local/bin/xdisplay` 或兼容 `xdisplay.sh`；
2. 安装 `~/.local/bin/innogpu-restore-dp1-mode-x11`；
3. 安装 `~/.config/x11/innogpu-display-session.sh`；
4. 仅在缺少现有 watcher 启动项时，向 `xprofile` 追加带边界标记的 source 块；
5. 重复执行保持幂等，不覆盖 dotconfig 的命令、库、配置或测试。

`prepare-soft-xorg-dwm.sh` 和 patched-17 安装器可以调用该接入脚本，但不会在安装时启动 watcher 或
改变当前 RandR 布局。目标用户尚未安装 dotconfig 显示引擎时，接入脚本应明确失败，不能悄悄安装
仓库中的旧副本；上层驱动/Xorg 安装流程只记录警告并继续，避免可选用户组件阻断基础驱动恢复。

## 验证

Innogpu 仓库只运行接入测试：

```sh
tests/xdisplay/run-install-tests.sh
```

该测试验证：缺少 dotconfig 引擎时拒绝接入、设备钩子和会话文件安装正确、现有 xdisplay 不被修改、
重复安装幂等、已有 watcher 不被重复注册，以及符号链接 `xprofile` 不被替换。

xdisplay 的状态、适配器、多外屏、配置和自定义布局回归测试必须在 dotconfig 仓库运行：

```text
.local/share/test/display/xdisplay-adapter.sh
```

实机验收仍需同时观察 `xdisplay status`、`xrandr --current` 和物理屏幕；`connected` 本身不能证明
屏幕已出图。需要 modeset 的热插拔、合盖和多外屏测试不得混入普通安装测试。

## 驱动事故归档

fbterm、framebuffer mmap、Deepin 用户态 ABI、shader 固件和 PVR services 属于驱动运行时问题，
不属于显示布局引擎设计。对应权威记录为：

- [`patched-17：fbdev mmap`](../incidents/patched-17-fbdev-mmap.md)
- [`patched-18：用户态混配`](../incidents/patched-18-userspace-mix.md)
- [`patched-18：shader 固件`](../incidents/patched-18-shader-firmware.md)
- [`patched-20：运行时验收`](../incidents/patched-20-runtime.md)
