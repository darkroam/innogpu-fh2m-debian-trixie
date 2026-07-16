# 显示切换使用说明

## 自动行为

目标 watcher 在 X11 会话中自动处理登录、热插拔和开合盖：开盖时以内屏为主并向右扩展外屏；
合盖且外屏可用时先确保外屏出图，再关闭内屏；拔出外屏后清理 stale geometry。

当前本机 dotfiles 已验证该行为，但本项目代码吸纳尚未完成。使用前先查看
`../project/display-management.md` 的当前状态。

## 查看状态

```sh
xdisplay.sh --status
xrandr --current
```

重点检查：`health`、`stale_outputs`、`pending_outputs`、输出 geometry、target/current mode 和
framebuffer 尺寸。

## 手动布局

`displayselect` 是交互入口。它与 watcher 共用 apply lock，避免同时写 RandR。Arandr 分支用于
自由布局；来源实现尚未完成 manual marker，因此物理拓扑未变化时的长期手动所有权仍是挂起项。

## 故障判断

- RandR 在约 1 秒内正确但屏幕恢复明显更慢：优先记录为驱动或显示链路重新同步，不要无限提高 watcher 频率。
- 输出 `disconnected` 但仍有 geometry：属于 stale 输出，应由 watcher 显式关闭。
- 输出 connected 但没有模式：属于 pending，保留安全活屏并等待有限重试。
- 合盖直接挂起：检查 logind 电源策略，不是 RandR 布局脚本能够单独解决的问题。
