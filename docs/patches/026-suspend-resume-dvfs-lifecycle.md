# patch-026-suspend-resume-dvfs-lifecycle：devfreq/PVR 电源生命周期同步

## 状态

本补丁进入 `4.0.2-i1`、`4.0.2-i2` 和 `4.0.2-i3`。i1 已在 R11 deep 冒烟中失败，只保留历史复现入口，
不得安装或交付；i2 在其上增加 patch-028，i3 再增加 patch-029。i3 已安装并通过 R14 6/6 deep
正式矩阵，现为当前运行和交付版本；`4.0.0-i1` 为首层回退。

本补丁与已经实机验收的历史
[`026-inactive-crtc-vblank-guard.patch`](patch-026-inactive-crtc-vblank-guard.md) 无关。二者都保留，
必须使用完整文件名区分；新补丁不覆盖、替代或重新解释历史 patch-026。

## 根因与 patch-024 边界

R10 在 `4.0.1-i3` 上复现 deep 失败：`PVRSRVDevicePreClockSpeedChange()` 在
`InnogpuSysDevPostPowerState(0 -> 1)` 之前进入，`PVRSRVPowerLock()` 返回
`PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF`，PVR Server Errors 从 0 增至 1，机器未正常恢复。

反汇编确认 `PVRSRVDefaultDomainPower()` 只是对设备节点偏移 `0x6c` 的无锁读取；
`PVRSRVPowerLock()` 则先获取偏移 `0x58` 的锁，再检查同一电源状态。patch-024 的锁外快速
检查与后续 PreClock 取锁之间存在 TOCTOU 窗口，因此仅作为防御性快速路径保留，不能独立
证明或修复 deep 生命周期竞态。

## 同步语义核实

核实对象为运行内核对应的 Debian `linux-source-6.12` `6.12.101-1`：下载包 SHA-256
`9a5ea91bb8dc78a2a43f190de3c60f44cad91d6164556d0a3676aa1024342303`，其中
`drivers/devfreq/devfreq.c` SHA-256
`d22d2cfd76dda338aeff9e24d722a012ccde730f3f37f829983359c47184ba09`。

- `devfreq_monitor()` 持有 `devfreq->lock` 调用 `update_devfreq()`，driver target 在该调用内执行；
- `devfreq_monitor_suspend()` 获取同一 mutex、设置 `stop_polling=true`，随后调用
  `cancel_delayed_work_sync()`；因此它等待正在执行的 target 完成，并排空后续 polling work；
- `devfreq_suspend_device()` 向 governor 发送 `DEVFREQ_GOV_SUSPEND`，本驱动 governor 在该事件中
  调用 `devfreq_monitor_suspend()`；
- `SuspendDVFS()` 先将 `bEnabled=false`，再调用 `devfreq_suspend_device()`；失败可能已经增加
  `suspend_count`，必须由 `ResumeDVFS()` 恢复 `bEnabled` 并平衡计数。

结论：当前 6.12 路径不需要另造 PowerLock、引用计数或等待队列；调用 `SuspendDVFS()` 本身已经
提供所需 drain。依据：[Debian linux 6.12.101-1 devfreq.c](https://sources.debian.org/src/linux/6.12.101-1/drivers/devfreq/devfreq.c/)。

## 修改

`patches/026-suspend-resume-dvfs-lifecycle.patch` 只修改 `innosrvkm/pvr_drm.c`：

1. `pvr_pm_suspend()` 在 `PVRSRVDeviceSuspend()` 前调用 `SuspendDVFS()`；
2. `SuspendDVFS()` 自身失败时调用 `ResumeDVFS()`，恢复 flag 并平衡可能已增加的 suspend count；
3. PVR suspend 失败时同样调用 `ResumeDVFS()`，使未下电设备恢复调频；
4. `pvr_pm_resume()` 先完成 `PVRSRVDeviceResume()`，仅成功后调用 `ResumeDVFS()`；PVR 上电失败时
   保持 DVFS suspended，避免再次访问未上电设备；
5. 只在 `SUPPORT_LINUX_DVFS && LINUX_VERSION_CODE >= 5.0` 使用新路径。旧内核已经通过
   `pfnDvfsSuspend/pfnDvfsResume` 回调管理 DVFS，继续走原路径，避免双重 suspend/resume。

不修改 `drivers/` 跟踪源码、封闭对象、ABI、结构布局、显示路径或 patch-025-display。

## 构建与验证边界

失败候选 `4.0.2-i1 = patch-024 + patch-026-suspend-resume-dvfs-lifecycle`，固定 epoch
`1788624000`（2026-09-06 00:00 +0800）。R11 证明 patch-026 虽然关闭了 devfreq 并发源，
独立的 HAL 温度 work 仍会在 PVR 子设备恢复前进入 PreClock，因此 i1 不得安装或交付。
当前交付版本 i3 在 i2 基础上增加 [patch-029](029-suspend-resume-ddcci-panel.md)，
固定 epoch `1788796800`；i2 的固定 epoch 为 `1788710400`。display patch-025 不进入 i1、i2 或 i3。

静态 fixture 验证补丁零 fuzz 应用、单文件范围、suspend/resume 顺序、两条 suspend 失败回滚、
resume 失败不提前恢复 DVFS、旧内核防双调、版本/epoch 失败关闭和编译/包源码双接线。

2026-09-03 阶段 1 离线验证结果：

- 两个隔离输出的 deb 逐字一致，SHA-256 均为
  `e115bdcdffe746f41934d69a61ec03687ed257340db733ad51ba109a3020c09e`；
- 从最终 deb 解包的 DKMS 源码再次编译通过，vermagic 为
  `6.12.101+deb13-amd64 SMP preempt mod_unload modversions`，modversions 表含 769 项；
- 包边界通过，完整载荷不含 `.orig/.rej/.o.cmd`；包内 `pvr_dvfs_device.c` 与 patch-024 结果一致，
  `pvr_drm.c` 与 lifecycle 026 结果一致，`innodpu_drm_pm.c` 保持原始状态，证明未混入 display 025。

上述 i1 离线检查不能覆盖 R11 后续发现的温度 work 竞态，也不能替代真机 deep。i2 的构建与
验证状态见 patch-028 文档。i3 后续在用户本人在场、回退包和 RTC/物理兜底就绪的条件下完成
R13 冒烟与 R14 6/6 deep 正式矩阵；每轮 PVR 八项不增长，未出现目标 PowerLock/POWERED_OFF
错误，画面、键鼠和 TTY 均由用户确认。该结论属于 024/026/028/029 组合，不单独归因于本补丁。
