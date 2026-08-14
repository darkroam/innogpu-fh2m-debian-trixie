# Patch 002：DP fbcon fallback

## 目的

在 DP 输出能力或启动时序不完整时提供安全的 framebuffer console fallback，避免把临时探测失败
直接扩散为黑屏。

## 实现

- 代码：`patches/002-dp-fbdev-fallback-mode.patch`。
- 开关：`APPLY_DP_FBCON_FALLBACK=1`。
- 只作用于驱动启动和 connector fallback，不改变 X11 watcher 的布局策略。

## 验证与边界

已在本机启动链路中使用。该补丁不是通用的多屏布局实现；若 DP 能力仍未就绪，必须保留可见的
安全输出并等待后续探测。
