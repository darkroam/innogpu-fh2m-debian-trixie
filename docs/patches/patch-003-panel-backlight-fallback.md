# Patch 003：面板背光 fallback

## 目的

当厂商面板背光接口不可用时提供可控 fallback，避免面板初始化失败阻塞图形输出。

## 实现

- 代码：`patches/003-panel-backlight-fallback.patch`。
- 开关：`APPLY_PANEL_BACKLIGHT_FALLBACK=1`。
- 只处理驱动背光能力，不替换用户态亮度工具或 X11 配置。

## 验证与边界

已纳入本机完整构建流程。若厂商接口恢复，应优先使用真实接口；fallback 不应被扩展为强制覆盖
所有平台的背光策略。
