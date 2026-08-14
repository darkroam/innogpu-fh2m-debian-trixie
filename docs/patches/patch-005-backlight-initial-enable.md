# Patch 005：背光初始 enable

## 目的

在设备已完成注册但初始背光状态未正确打开时，确保首次可见输出不会停留在全黑状态。

## 实现

- 代码：`patches/005-backlight-force-initial-enable.patch`。
- 开关：`APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=1`。
- 仅作用于初始化时的安全状态，不持续覆盖用户亮度设置。

## 验证与边界

已在本机启动和显示验证中使用。后续修改必须分别检查启动亮度、用户调节和合盖恢复。
