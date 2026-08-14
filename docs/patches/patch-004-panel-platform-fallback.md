# Patch 004：面板平台 fallback

## 目的

在面板平台设备注册缺失或时序不完整时提供保守的注册回退，保持驱动能完成基础启动。

## 实现

- 代码：`patches/004-panel-platform-fallback.patch`。
- 开关：`APPLY_PANEL_PLATFORM_FALLBACK=1`。
- 不修改 RandR 输出命名和布局算法。

## 验证与边界

已通过本机 DKMS/启动验证。回退路径只用于缺失能力的设备，不能替代对新面板型号的独立验证。
