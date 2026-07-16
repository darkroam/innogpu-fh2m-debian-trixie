# 显示管理

## 当前状态

本机已经在 dotfiles 配置中完成新一轮 X11 显示管理改造，并通过热插拔、开合盖、当前会话换代和
整机重启验证。来源基线为 dotfiles 提交 `5628c6e`，核心文件为 `~/.local/bin/xdisplay.sh`。

本项目仓库目前仍安装 `scripts/xdisplay.sh.with-innogpu-restore` 这一旧实现，它硬编码
`HDMI-1/eDP-1`，缺少统一 RandR 状态、stale 清理、模式能力跟踪和有界重试。因此在完成
`../planning/display-integration.md` 前，本机行为已验证不等于仓库安装流程已经包含该行为。

## 已验证来源实现

来源 watcher 每轮只解析一份 RandR 快照，记录：

- connection、primary、geometry 和正负坐标；
- current、preferred、target 模式及刷新率；
- 模式数量和能力签名；
- active、stale、pending 状态；
- lid 状态、物理拓扑签名和基础 health。

它显式关闭 `disconnected + geometry` 的 stale 输出，并在没有 preferred 时选择 RandR 模式表首项，
不再让 `--auto` 猜测。连接或模式能力变化后进入约 5 秒 settling 窗口；相同失败最多连续写入三次，
随后只在低频主动探测时恢复尝试。

## 运行关系

```text
innogpu + Xorg
  -> RandR outputs/modes/geometry
  -> xdisplay.sh --watch
       -> /proc/acpi/button/lid/*/state
       -> /sys/class/drm/card*-*/status
       -> xrandr --current / --query
       -> 串行应用并验证布局

displayselect -> 同一 apply lock -> xrandr
```

watcher 应由 X11 会话启动并继承 `DISPLAY`、`XAUTHORITY` 和用户环境。不要另建 udev 直接调用
`xrandr` 的链路，也不要在没有完整图形会话环境的 systemd 服务中启动第二个 watcher。

## 本设备边界

- DRM `DP-1`、`HDMI-A-1`、`HDMI-A-2` 当前映射为 RandR `eDP-1`、`HDMI-1`、`HDMI-2`。
- 通用代码不硬编码外屏；本设备仅补充内屏候选 `eDP-1 DP-1`。
- `restore-dp1-mode-x11.sh` 带固定 modeline，只是 Innogpu 设备恢复钩子，不是通用布局策略。
- logind 决定合盖是否挂起；watcher 只在 X11 会话仍运行时处理布局。
- framebuffer 应收敛到有效输出包围盒。保留旧 framebuffer 只能用于受控诊断，不能成为默认策略。

## 已验证场景

来源实现已验证：开盖热插、热拔、合盖外屏、再次开盖、开盖后再次拔出、stale geometry 清理、
扩展坞能力延迟、没有 preferred 的显式目标模式，以及 watcher 当前会话受控交接。

仍需在本项目吸纳后重新验证，不能直接沿用来源仓库的 pass 结果。至少包括：

- fixture 中的状态、生命周期和回归测试；
- 当前 X11 `--status` 与只读状态比较；
- 受控 watcher 交接；
- 开盖、合盖、热插和热拔；
- `prepare-soft-xorg-dwm.sh` 不再覆盖新 watcher；
- 新设备安装说明中的启动入口真实有效。

## 状态接口

计划吸纳的命令接口为：

```text
xdisplay.sh [--apply]
xdisplay.sh --watch
xdisplay.sh --status
```

`--status` 必须只读。来源计划中的 `--manual-run`、manual marker 和单设备适配器尚未完成，不能在
本项目文档中提前声明为可用。
