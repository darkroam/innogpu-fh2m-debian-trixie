# Patch 006：本机 ACPI DPU 映射恢复

## 目的

当厂商 hwinfo 无法提供有效 DPU 匹配值时，从本设备 ACPI `_DSD` 的厂商 GUID 属性读取 `DPU_MATCH`，
使驱动选择正确的 connector 表，并补齐本机接口最大分辨率和 connector 索引处理。

## 实现

- 代码：`patches/006-local-connector-acpi-map.patch`。
- 开关：`APPLY_LOCAL_CONNECTOR_ACPI_MAP=1`。
- ACPI 读取失败或返回非正值时，仍回退到原有 `fh2m_hal_get_dpu_match()`。
- 本补丁不直接定义 RandR 输出名，也不实现 X11 布局。

## 验证与边界

这是本机固件/connector 表特例，不应抽成通用外屏名称规则。当前观察到的 DRM `DP-1`、
`HDMI-A-1`、`HDMI-A-2` 与 RandR `eDP-1`、`HDMI-1`、`HDMI-2` 关系记录在显示接入文档中，属于
运行时观察，不是此 patch 写死的名称映射。新的设备必须先采集 ACPI `_DSD`、connector 和 RandR
拓扑，再单独设计和测试。
