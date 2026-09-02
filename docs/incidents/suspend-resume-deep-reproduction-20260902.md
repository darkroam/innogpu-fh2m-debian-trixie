# 2026-09-02 suspend/resume deep 再现与 s2idle 测试方法失效

## 现象

在当前运行包 `4.0.0-i1` 上尝试验证 s2idle 规避时，机器实际进入 deep S3。恢复后外屏整屏红色，
内屏和 SSH/TTY 均无法恢复，只能通过电源键触发关机并重启。

本次没有安装、构建或加载 patch-024；运行模块仍来自 `4.0.0-i1`。因此该事故不是候选补丁引入的
回归，而是已知 deep resume 故障的再次复现。

## 测试方法错误

测试脚本执行了以下顺序：

1. 把 `/sys/power/mem_sleep` 写为 `s2idle`，读取结果为 `[s2idle] deep`；
2. 调用 `systemctl suspend`；
3. 假定该命令会阻塞到唤醒，并在命令返回后的 EXIT trap 中恢复原模式 `deep`。

实测 `systemctl suspend` 在本环境中约 17 ms 即返回，而内核约 2 秒后才记录 suspend entry。trap 因此
在真正挂起前恢复了 deep，最终日志明确是 `PM: suspend entry (deep)` 与 ACPI S3。该方法不能用于
验证 s2idle，也不能用“命令返回”代表已经完成一次挂起/恢复。

## 内核时间线（已脱敏摘要）

```text
10:39:45.895  测试开始，切换前 s2idle [deep]
10:39:45.913  systemctl suspend 已返回，trap 恢复 deep
10:39:47      PM: suspend entry (deep)
10:39:59      InnogpuSysDevPrePowerState current=1 new=0
10:39:59      ACPI: PM: Preparing to enter system sleep state S3
10:39:59      ACPI: PM: Waking up from system sleep state S3
10:39:59      PVRSRVPowerLock failed (PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF)
               in PVRSRVDevicePreClockSpeedChange
10:39:59      InnogpuSysDevPostPowerState current=0 new=1
10:39:59      PM: suspend exit
10:41:19      用户通过电源键触发关机
```

PowerLock 失败仍发生在 `PostPowerState(OFF->ON)` 之前，与 R02 原始 deep 事故完全同序，进一步支持
patch-024 的门禁位置；但这不等于补丁已通过真机验证。

## 结论与边界

- deep 故障：在未修改的 `4.0.0-i1` 上再次复现，外屏红屏和本地/网络恢复失败是实际影响。
- s2idle：**没有完成有效测试**，不能写成成功、失败或可规避。
- patch-024：本次未运行，不能据此判断修复效果。
- 后续禁止再次使用“写 sysfs + `systemctl suspend` 返回后 trap 恢复”的方法。

下一次有副作用测试必须同时满足：先安装独立升号的候选包；测试方案能在真正 suspend entry 时保持
目标模式；由外部观察者确认 journal 中的 `PM: suspend entry (s2idle|deep)`；准备物理电源键和已验证
回退包。该方案需单独复审后执行，本轮不再进行挂起。
