# patch-028-suspend-resume-hal-temp-monitor-delay：温度监控 work 恢复时序门禁

## 状态

`4.0.2-i2` 的静态/离线候选修复，尚未安装或执行 deep 真机验收，不能声称问题已修复。
当前运行与回退基线仍是 `4.0.0-i1`。R11 已证明 `4.0.2-i1` deep 失败，因此该版本只保留
历史复现入口，**不得安装或交付**。display patch-025 继续保持 **UNVERIFIED**，不进入 i2。

本补丁编号为 028，避免与已发布历史补丁
[`patch-027-foreign-dmabuf-lifecycle`](patch-027-foreign-dmabuf-lifecycle.md) 冲突。

## R11 失败证据与根因

R11 在 `4.0.2-i1`（patch-024 + patch-026）上真实进入并退出 deep，但恢复画面失败，
PVR Server Errors 从 0 增至 1。journal 的关键顺序是：

```text
PVRSRVPowerLock() failed (PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF)
in PVRSRVDevicePreClockSpeedChange()
hal_temperature_monitor_work: recover gpu pll is: 1350
InnogpuSysDevPostPowerState() state: current=0, new=1
```

封闭 HAL 的反汇编表明：`hal_power_sleep()` 同步取消并排空温度监控 delayed work；
`hal_power_wakeup()` 随后以 delay 0 重新排队；work 在恢复 GPU PLL 前后通过
`fh2m_hal_gpudrv_clkchange(..., PRE/POST)` 进入 PVR 时钟变更回调。可审查源码原先从 PCI
父设备 resume 调用 `hal_power_wakeup()`，而 Linux PM 恢复顺序是父设备先于子设备；此时
PVR 子 platform device 的 `pvr_pm_resume()` 尚未完成。patch-026 只同步了 devfreq monitor，
没有覆盖这个独立 workqueue，因此 i1 仍可在 PVR system power 为 OFF 时进入 PreClock。

`chip.pm_resume()` 只执行父 PCI/芯片硬件恢复；其返回不等待 PVR 子设备 PM callback，不能作为
PVR 已上电的同步点。补丁因此不在该调用后简单重排，而是在 PVR 子设备恢复成功处建立门禁。

## 修改

`patches/028-suspend-resume-hal-temp-monitor-delay.patch` 在 patch-024 与 patch-026 之后应用，
只修改三个可审查源码文件：

1. 在 `struct dev_rsrc` 尾部追加每父设备独立的 `pvr_resume_count`。追加而非插入，保证闭源
   HAL 已有字段的偏移不变；该结构由可审查 PCI probe 按 `sizeof(struct dev_rsrc)` 零初始化分配。
2. S3 suspend 在 `hal_power_sleep()` 完成后把计数归零，保留原有同步 cancel/flush 语义。
3. PCI 父设备的 S3 resume 不再调用 `hal_power_wakeup()`；hibernate thaw 的既有行为保留。
4. 每个 PVR 子设备先成功完成 `PVRSRVDeviceResume()`，在当前内核上再成功完成 patch-026 的
   `ResumeDVFS()`，然后才增加所属父设备的计数。最后一个 PVR 子设备恢复后才启动温度 work。
5. PVR 或 DVFS resume 失败、平台数据无效、计数溢出及 `NO_HARDWARE` 路径均不会新增
   温度 work 启动。旧内核路径仍以 `PVRSRVDeviceResume()` 成功作为门禁，不重复调用 DVFS。

补丁不修改 `drivers/` 工作树、封闭对象、显示恢复 patch-025 或 patch-026 的 suspend 顺序。

## 构建与验证边界

`4.0.2-i2 = patch-024 + patch-026-suspend-resume-dvfs-lifecycle + patch-028`，固定 epoch
`1788710400`（2026-09-07 00:00 +0800）。`4.0.2-i1` 的 epoch `1788624000` 仅供失败候选
历史复现，构建器默认入口已切换到 i2。

静态 fixture 覆盖严格零 fuzz 应用、补丁范围、既有 `dev_rsrc` 字段偏移不变、父/子调用顺序、
多父/多 PVR 子设备计数、PVR/DVFS 失败、旧内核、`NO_HARDWARE`、hibernate 保留、版本与 epoch
失败关闭，以及编译 staging 与包内 DKMS 源码双接线。离线构建只能证明补丁可应用、可编译、
包内容符合边界且可复现，不能证明 deep 恢复已修复。

2026-09-03 阶段 1 离线验证结果：

- 两个隔离输出的 deb 逐字一致，SHA-256 均为
  `9d5d427dfcce6c620a1efae74353657094e4d8ede0924b2efc16b4fb9cec244f`；
- 从最终 deb 解包的 DKMS 源码再次编译通过，vermagic 为
  `6.12.101+deb13-amd64 SMP preempt mod_unload modversions`，modversions 表含 769 项；
- 包边界通过，完整载荷不含 `.orig/.rej/.o.cmd`；包内 024、026、028 状态均符合预期，且
  `innodpu_drm_pm.c` 仍有原始 post-atomic 光标恢复，证明未混入 display 025；
- 编译输出含 4422 条厂商源码/封闭对象既有 warning；patch-028 涉及的新增语句未产生编译错误，
  该计数不作为真机正确性证据。

阶段 2 必须另获用户在场批准：在接电、无外屏、合盖条件下连续执行 3 次受控 deep，每轮均须
有成对 `PM: suspend entry (deep)`/`PM: suspend exit`，画面、键鼠和 TTY 恢复，无 `3900372`、
`PVRSRVPowerLock`、`POWERED_OFF`、`PVR_K:(Error)`，PVR 八项计数不增长，并取得温度 work
恢复运行的证据。任一失败立即停止并回退 `4.0.0-i1`；SSH 没有独立客户端时只能记
`NOT_TESTED`。
