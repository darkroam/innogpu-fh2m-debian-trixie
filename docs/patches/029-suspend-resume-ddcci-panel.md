# patch-029-suspend-resume-ddcci-panel：DDCCI panel 创建恢复

## 目的与边界

patch-029 是 R13 B 线的最小候选，针对 `hwinfo_g0m.bin` 缺失时驱动回退到
`CONNECTOR_BACKLIGHT_DDCCI`、却因此不创建 panel 的路径。它只恢复 panel 电源和
backlight-enable callback 的可达性，不声称已经实现 DDCCI 亮度控制。

本补丁只有两处行为变化：

1. `innodpu_dp_debugfs.c` 将 panel 创建条件下界从 `CONNECTOR_BACKLIGHT_PWM0`
   放宽到 `CONNECTOR_BACKLIGHT_DDCCI`。
2. `inno_panel_backlight_init()` 对 DDCCI 显式返回 0，使 panel 保持
   `backlight == NULL`，不分配或注册 backlight device。

`connector->force = DRM_FORCE_ON` 仍只在原来的 `PWM0..AUX_HDR` 范围内设置；DDCCI
不被静默转换为 PWM，也不伪造 brightness sysfs。没有扩展任何函数签名。

## 原因链与诚实边界

R12 journal 已记录 `hwinfo_g0m.bin` 加载失败后回退 DDCCI；当前驱动的 panel 创建
条件排除了枚举值 0，导致 `dp_dev->panel == NULL`，panel 的 GPIO callback 不可达。
patch-029 使 DDCCI 能创建 panel，但保留无 backlight device 的语义。6.12 的
`backlight_enable(NULL)` 返回 0，因此 panel enable callback 仍可继续执行。

DDCCI backlight device 是后续独立能力项，不属于本补丁：当前没有足够的 FH2M
DDC/CI 目标地址、VCP 命令、响应格式、亮度映射和 resume 时序证据，不能仅凭模式
枚举注册一个会暴露 brightness sysfs 的伪实现。

## 版本与应用

`4.0.2-i3` 固定为 `patch-024 + patch-026 + patch-028 + patch-029`，使用审核通过
的 `SOURCE_DATE_EPOCH=1788796800`（`2026-09-08 00:00 +0800`）。patch-025 不进入
i3；i2 保留 `024 + 026 + 028` 的历史复现集合。

构建器在编译 staging tree 和包内 DKMS source tree 均以
`--fuzz=0 --no-backup-if-mismatch` 应用补丁，并拒绝 `.orig`/`.rej` 产物。

## 静态与实机门禁

静态 fixture 覆盖 DDCCI panel 正例、PWM 正例、非法模式负例、DDCCI 不注册设备、
空 backlight wrapper 可达性和 `force` 语义不变。阶段 1 只做 dry-run、离线构建和
包边界检查，不安装、不重启、不执行 deep。

安装、重启和 deep 恢复属于后续独立批准；实机阶段必须确认 panel/GPIO 路径、无伪造
backlight sysfs 设备、probe 日志与显示结果，再决定是否扩大验证矩阵。

## 当前状态

这是静态/离线候选，尚未安装或执行 deep 真机验收，不能据此声称 suspend/resume
显示恢复问题已经修复。完成 qoder 初审和 dsh 终审并提交前，仍不得交付或安装。
