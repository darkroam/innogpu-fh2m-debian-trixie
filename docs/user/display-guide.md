# 显示接入使用说明

## 所有权

xdisplay 的命令、共享库、配置、状态机、布局策略和内部测试全部由 dotconfig 维护。本项目不保存
这些代码，只提供 Innogpu 设备的输出候选、固定模式恢复钩子和 X11 会话接入。权威边界见
[`../project/display-management.md`](../project/display-management.md)。

## 安装接入

先从 dotconfig 安装 xdisplay，再执行：

```sh
sudo INNOGPU_X_USER="$USER" INNOGPU_X_HOME="$HOME" \
  scripts/install-xdisplay-user.sh
```

该命令不会启动 watcher，也不会改变当前 RandR 布局。它只安装设备钩子，并在尚无 watcher 入口时
向 `xprofile` 追加带边界标记的会话片段。

## 查看状态

```sh
xdisplay status
xrandr --current
```

实机判断应同时观察物理屏幕。输出 `connected` 但没有模式属于 pending；`disconnected` 仍带 geometry
属于 stale；这两类通用状态的处理由 dotconfig xdisplay 决定。

## 手动布局

`displayselect` 是 dotconfig 提供的交互和自定义布局入口，并与 watcher 共用 apply lock。命令参数、
保存/恢复规则和配置格式以 dotconfig 的 `display-device-adapter.md` 为准，本项目不复制其说明。

## Innogpu 特例

本机的 `DP-1` 需要固定 modeline 恢复时，xdisplay 通过以下兼容契约调用设备钩子：

```text
XDISPLAY_INTERNAL_OUTPUTS="eDP-1 DP-1"
XDISPLAY_RESTORE_COMMAND=innogpu-restore-dp1-mode-x11
```

钩子失败后，通用引擎负责降级并优先保留可见输出。合盖是否挂起仍由 logind 决定，不属于 RandR
布局脚本可以单独解决的问题。
