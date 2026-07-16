# 显示管理

## 当前状态

仓库已吸纳 dotfiles 提交 `5628c6e` 的 X11 显示管理实现。`scripts/xdisplay.sh` 与来源文件保持
完全一致，SHA-256 为 `427d56f78ff11482c59c9e4b95f9fc75a1890ca83b83d81956410d17e6690251`；
`displayselect` 在来源基础上仅将个人桌面后处理改为可选。旧的
`scripts/xdisplay.sh.with-innogpu-restore` 已删除。

## 当前实现

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

用户文件由 `install-xdisplay-user.sh` 安装，`prepare-soft-xorg-dwm.sh` 只负责调用它并继续完成
系统级软渲染准备。安装器只管理 `~/.local/bin` 中的显示命令、
`~/.config/x11/innogpu-display-session.sh` 和 `xprofile` 中带边界标记的启动块；检测到已有
`xdisplay.sh --watch` 时不得追加第二个 watcher。

`displayselect` 的 RandR 布局操作不依赖个人 dotfiles。壁纸刷新、键盘重映射和通知守护进程重启
属于可选桌面后处理；`setbg`、`remaps`、`dunst` 或 `notify-send` 不存在或失败时必须跳过，
不能把已成功的显示切换报告为失败。

## 本设备边界

- DRM `DP-1`、`HDMI-A-1`、`HDMI-A-2` 当前映射为 RandR `eDP-1`、`HDMI-1`、`HDMI-2`。
- 通用代码不硬编码外屏；本设备仅补充内屏候选 `eDP-1 DP-1`。
- `restore-dp1-mode-x11.sh` 带固定 modeline，只是 Innogpu 设备恢复钩子，不是通用布局策略。
- logind 决定合盖是否挂起；watcher 只在 X11 会话仍运行时处理布局。
- framebuffer 应收敛到有效输出包围盒。保留旧 framebuffer 只能用于受控诊断，不能成为默认策略。

## 验证状态

来源实现已验证：开盖热插、热拔、合盖外屏、再次开盖、开盖后再次拔出、stale geometry 清理、
扩展坞能力延迟、没有 preferred 的显式目标模式，以及 watcher 当前会话受控交接。

仓库吸纳后已重新验证：

- 基础状态和锁测试 12 项、watcher 生命周期 4 项、显示回归 11 项全部通过；
- 临时 HOME 中的用户安装器 4 项测试通过；
- patched-17 测试包从 patched-8 与 Deepin 202504 deb 完整重建，包内显示文件与仓库一致；
- 当前 X11 中仓库与已安装 watcher 的 `--status` 输出一致，合盖外屏状态为 `health=ready`，
  无 stale 或 pending 输出；
- 当前 watcher 已运行来源相同的代码，因此本轮未做无意义的 watcher 热交接，也未重复执行会改变
  当前布局的热插拔、开合盖或 `--apply` 测试。实机 modeset 结论仍引用上述来源验证。

## 状态接口

当前命令接口为：

```text
xdisplay.sh [--apply]
xdisplay.sh --watch
xdisplay.sh --status
```

`--status` 必须只读。来源计划中的 `--manual-run`、manual marker 和单设备适配器尚未完成，不能在
本项目文档中提前声明为可用。
