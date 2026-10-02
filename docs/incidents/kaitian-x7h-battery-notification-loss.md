# KaiTian X7h G1e 电池状态丢失

日期：2026-10-02

## 现象

插电启动时 `BAT0` 正常显示；拔线后电池对象消失；插回后只恢复 `ADP1`，`BAT0` 不重建；重启重新 probe 后恢复。

## 证据

- 机器 DMI：`KaiTian / X7h G1e / W0WKT18A`。
- DSDT `BAT0._STA` 仅在 `ECON == 1` 且 `BA1P & 1` 时返回 `0x1F`，否则返回 `0x0F`。
- DSDT `_Q25` 立即通知 `ADP1`、`BAT0` status 和 `BAT0` information；`_Q37/_Q38` 先 `Sleep(0x012C)`（300 ms）再通知 `ADP1` 和 `BAT0`。
- 特权现场采集显示：拔线时先有 `BAT0` power-supply remove，约 1 秒后才有 `ADP1 online=0`；插回只产生 `ADP1 online=1`，没有 BAT0 add/change。
- 通过 battery 驱动 unbind/bind 可在不重启下恢复 `BAT0`，证明 ACPI 设备仍在、缺失的是 power-supply 注册对象。

## 根因与修复边界

根因归类为厂商 ACPI/EC 时序缺陷：EC 状态在 BAT0 通知处理时不稳定，Linux battery 驱动按 `_STA` 结果合法移除 power-supply；后续没有有效 BAT0 通知触发重建。不是通用内核通知分发缺陷。

第三方 workaround：[`components/linux/002-kaitian-x7h-battery-notification-delay.patch`](../../components/linux/002-kaitian-x7h-battery-notification-delay.patch) 复用 Linux 现有 `battery_notification_delay_ms=1000` 机制，仅匹配该机型的 DMI。适用基线为 Debian `linux-source-6.12` `6.12.107-1`；换用其他内核版本必须重新检查上下文/API 并离线编译验证。它不修改 ACPI、通用 PM 顺序或其他机型。

该补丁已通过 `patch --dry-run --fuzz=0`、`olddefconfig` 和 `drivers/acpi/battery.o` 离线编译；尚未安装或运行验证。优先修复路径仍是 BIOS/EC/DSDT；补丁是本机内核兜底，安装需另行授权。

没有该内核补丁时，现场可先运行 [`scripts/restore-bat0.sh`](../../scripts/restore-bat0.sh)。脚本从普通用户
启动并自动提权，记录完整证据，然后对 ACPI `PNP0C0A:*` battery 设备执行 unbind/bind；它不要求
电源线处于某一状态，也不卸载模块、不重启、不触发 PM。脚本只能重新探测仍由 ACPI/EC 暴露的电池，
不能在固件完全不暴露电池设备时伪造 `BAT0`。

## 维护要求

未来更换内核基座时必须重新检查该 DMI 机型是否仍需要此 workaround，并复跑一次拔电/插电序列；不得因新内核能启动就删除补丁或把固件缺陷当作已消失。
