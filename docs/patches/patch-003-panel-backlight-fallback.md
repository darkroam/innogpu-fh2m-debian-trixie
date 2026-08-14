# Patch 003：面板背光 fallback

## 目的

当厂商面板背光接口不可用时提供可控 fallback，避免面板初始化失败阻塞图形输出。

## 实现

- 代码：`patches/003-panel-backlight-fallback.patch`。
- 开关：`APPLY_PANEL_BACKLIGHT_FALLBACK=1`。
- 只处理驱动背光能力，不替换用户态亮度工具或 X11 配置。

## 验证与边界

该补丁曾完成本机验证，但 patched-19/20 固定 wrapper 将开关设为 `0`，当前候选没有启用。若后续
重新启用，必须单独验证启动、亮度调节和恢复；fallback 不应被扩展为强制覆盖所有平台的背光策略。
