# 2026-09-02 4.0.1-i1 s2idle 唤醒红屏与回退

## 现象

`4.0.1-i1` 安装并重启后完成一次真实 s2idle 挂起。内核和自动化图形检查没有报告 PowerLock 或 PVR
错误，但用户观察到外接屏整屏红色，显示不可用。用户随后通过电源键触发有序关机并重启；没有执行
计划中的 deep 测试。

## 构建与回退前提

- 三份 `4.0.0-i1` 回退包逐字节一致，SHA-256：
  `68aea6c07842a0def97d18de5385802175290cb9752571b157250eb38fa68735`。
- `4.0.1-i1` 以固定 epoch `1788278400` 构建，离线 DKMS、包边界和包内 patch 检查通过，SHA-256：
  `a7fe10ed8946bee7a850d26151387fa43d01c8bf2ce6253d99a47133a39827e6`。
- 安装后重启验证了包版本、DKMS、vermagic、模块、Driver/Firmware、设备节点、Xorg 和硬件 GL；
  挂起前 PVR 错误计数为零。

## s2idle 时间线

测试期间 `/sys/power/mem_sleep` 保持 `[s2idle] deep`，使用 RTC 定时唤醒。保存的本机 journal 摘要为：

```text
11:55:10  PM: suspend entry (s2idle)
11:55:31  InnogpuSysDevPrePowerState current=1 new=0
11:55:31  InnogpuSysDevPostPowerState current=0 new=1
11:55:31  PM: suspend exit
```

该时间窗没有 `PVR_K 3900372`、`PVRSRVEPowerLock failed` 或 PVR 错误计数增长，SSH 服务和监听端口也
恢复。自动化 Xorg/GL 检查返回成功，但外屏实际为红屏，因此自动化结果不能覆盖用户可见失败。

## 关机与回退澄清

用户在显示不可用时按电源键，journal 于 11:59:19 记录 `systemd-shutdown` 同步文件系统并发送
`SIGTERM`，说明 SSH 断开来自有序关机，不是单独把 `mem_sleep` 改回 deep 直接导致。用户之后又执行
一次显式重启，最终完整安装 `4.0.0-i1` 回退包并重启。当前包、DKMS、模块、Driver/Firmware 和 PVR
错误计数均恢复到回退基线；`mem_sleep` 为 `s2idle [deep]`。

## 登录后短暂黑屏是独立问题

`4.0.1-i1` 登录桌面时曾观察到外屏亮一下、黑数秒后恢复；完整回退 `4.0.0-i1` 后同样复现，故排除
patch-024 特异回归。只读取证显示合盖登录时 xdisplay 启动唯一 watcher，把 Xorg 初始布局切为
`EXTERNAL_ONLY`：关闭 `eDP-1`、把 `HDMI-2` 设为 1920x1080 主屏。Xorg 日志在对应时间重建
1920x1080 framebuffer，并在随后约 5 秒重复读取输出信息，与可见闪黑一致。

该问题属于 dotconfig/xdisplay 会话布局切换，本仓库不修改其实现。后续应在独立轮次中研究避免登录后
二次 modeset；在有回退方案前不得在线反复试错 `xrandr`。

## 结论与边界

- 本次 s2idle 日志没有出现目标 PowerLock 失败，但因缺少 `4.0.0-i1` 的同方法 s2idle 对照，不能把
  “未出现”单独归因于 patch-024；且**没有恢复可用显示**，不能认定修复有效。
- s2idle 经有效测试仍出现红屏，**不是可用的短期规避方案**。
- `4.0.1-i1` 的 deep 未测试；B1 失败后取消 B2，不能推断 deep 结果。
- `4.0.1-i1` 是失败候选，不得继续安装。当前运行版本为 `4.0.0-i1`。
- 后续验收必须把人工可见画面、SSH、TTY 与机械日志共同作为门禁；任一失败即失败。

原始日志保留在本机忽略目录 `build/`，不进入 Git。
