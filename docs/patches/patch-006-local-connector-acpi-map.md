# Patch 006：本机 connector/ACPI 映射

## 目的

处理本设备 DRM connector 与 ACPI 输出命名不一致的问题，保证设备钩子能定位正确的内屏和外屏。

## 实现

- 代码：`patches/006-local-connector-acpi-map.patch`。
- 开关：`APPLY_LOCAL_CONNECTOR_ACPI_MAP=1`。
- 当前已知映射：DRM `DP-1`、`HDMI-A-1`、`HDMI-A-2` 对应 RandR `eDP-1`、`HDMI-1`、`HDMI-2`。

## 验证与边界

这是本机硬件特例，不应抽成通用外屏名称规则。新的设备必须先采集 connector、ACPI 和 RandR
拓扑，再单独增加映射和测试。
