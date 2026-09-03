# patch-024：suspend/resume devfreq 电源状态门禁

## 目的

修复 deep S3 唤醒期间 devfreq 在 PVR 默认电源域仍为 `OFF` 时进入
`PVRSRVDevicePreClockSpeedChange()` 的时序缺口。事故日志中的顺序为：

1. `PVRSRVDevicePreClockSpeedChange()` 获取 PowerLock；
2. 返回 `PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF`；
3. 随后才执行 `InnogpuSysDevPostPowerState(current=0,new=1)`。

该失败发生在 resume 瞬间，并伴随内屏、SSH 与 TTY 不可用；logind 仍能响应电源键并完成关机。
这说明 ACPI resume 和 userland 尚存活，故障集中在 GPU/显示恢复路径。

## 修改

`patches/024-suspend-resume.patch` 只修改 `innosrvkm/pvr_dvfs_device.c`。在
`devfreq_target()` 确认 DVFS 已启用后、查询 OPP 和调用
`PVRSRVDevicePreClockSpeedChange()` 前，通过 `PVRSRVDefaultDomainPower()` 查询 PVR 跟踪的默认
电源域状态：

- 状态为 `PVRSRV_SYS_POWER_STATE_ON`：完整保留原调频、PowerLock 与错误传播逻辑；
- 其他状态：不查询 OPP、不改电压/时钟，把 `requested_freq` 设为当前核心频率后返回；
- 上电完成后，后续 devfreq 轮询可重新发起正常调频。

门禁不改写电源状态，不改变 ioctl ABI、结构布局或锁实现，也不把 ON 状态下真实的 PowerLock 错误
改成成功。

## 构建接线

历史 `4.0.1-i1` 固定 epoch 为 `1788278400`（2026-09-02 00:00 +0800），只应用
patch-024。当前 R06 A=`4.0.1-i3` 仍只应用 patch-024，B=`4.0.1-i4` 再应用
patch-025-suspend-resume-display；两者共用 epoch `1788451200`，详见[独立说明](025-suspend-resume-display.md)。
R10 deep 失败证明该锁外门禁存在 TOCTOU，故 `4.0.2-i1/i2` 保留 patch-024 作为防御性快速路径，
并以 [patch-026 生命周期同步](026-suspend-resume-dvfs-lifecycle.md) 和
[patch-028 温度 work 门禁](028-suspend-resume-hal-temp-monitor-delay.md) 建立实际同步边界。

legacy `scripts/build-deepin-coherent.sh` 的开关为 `APPLY_SUSPEND_RESUME_FIX=1`，默认关闭；
`scripts/build-patched28-suspend-resume.sh` 继承 patched-27 的完整开关集合后增加 patch-024，只作为
旧构建链对照入口。

补丁编号 024 是补丁目录中的历史空缺；包版本 `patched-24` 已用于 Debian 6.12.101+ PCI API
兼容，不能复用。因此 legacy 对照版本使用 patched-28；正式新架构候选使用 `4.0.1-i1`，也不复用
当前已安装并验收过的 `4.0.0-i1`。

## 离线验证

`tests/unit/run-suspend-resume-tests.sh` 不读取本地载荷，验证：

- patch-024 对当前跟踪的 `pvr_dvfs_device.c` 可 `patch -p1 --dry-run` 并实际应用到临时副本；
- 电源状态门禁位于 OPP 查询及 PreClock 调用之前；
- 非 ON 分支保持当前频率并返回；
- 补丁只触及一个 driver 文件；
- 新架构构建器对 R06 i3/i4 固定同一 epoch，且编译树和包内源码都应用 patch-024；
- coherent 构建器与 patched-28 legacy 包装器的开关、版本和 p27 继承关系完整。

这只证明补丁可应用和接线正确，不证明真机恢复问题已经解决。

## 真机验证结果

2026-09-02 获得本地载荷只读授权后完成以下步骤：

1. 三份 `4.0.0-i1` 回退包逐字节一致，SHA-256 均为
   `68aea6c07842a0def97d18de5385802175290cb9752571b157250eb38fa68735`。
2. `4.0.1-i1` 以 epoch `1788278400` 构建成功，离线 DKMS 编译、包边界和包内 patch 检查通过；
   产物 SHA-256 为 `a7fe10ed8946bee7a850d26151387fa43d01c8bf2ce6253d99a47133a39827e6`。
3. 通过包安装并重启，没有热切模块；包版本、DKMS、vermagic、模块、Driver/Firmware、设备节点、
   Xorg 和硬件 GL 基础检查通过，PVR 错误计数为零。
4. B1 保持 `[s2idle] deep` 并由 RTC 在约 20 秒后唤醒。journal 独立记录
   `PM: suspend entry (s2idle)`、`InnogpuSysDevPrePowerState(1->0)`、
   `InnogpuSysDevPostPowerState(0->1)` 和 `PM: suspend exit`，没有 `3900372`、PowerLock 失败或 PVR
   错误计数增长；但是外屏实际为整屏红色，用户无法正常使用显示。因此用户可见结果优先于机械探针，
   B1 判定为 **FAIL**。
5. B1 失败后取消 B2 deep，不再进行任何挂起测试。机器经电源键触发有序关机后重启，随后完整回退
   `4.0.0-i1` 并再次重启；当前包、DKMS、模块和 Driver/Firmware 状态均已恢复。

原始日志只保留在本机忽略目录 `build/`，不提交到 Git。详见
[s2idle 红屏事故记录](../incidents/suspend-resume-s2idle-red-screen-20260902.md)。

## 当前结论

- patch-024 / `4.0.1-i1`：构建、安装、启动和 PVR 机械门禁通过，但真实 s2idle 唤醒后外屏红屏，
  **未能恢复可用显示，候选验收失败，不得继续安装**。
- s2idle：已完成一次有效 entry/exit 验证，但用户可见恢复失败，**不能作为短期规避方案**。
- deep：`4.0.0-i1` 已复现已知 PowerLock 时序故障；`4.0.1-i1` 因 B1 先失败而未执行 deep 测试，
  不能声称 patch-024 修复了 deep。
- 当前运行版本已回退为 `4.0.0-i1`。后续修复必须扩大到完整显示恢复时序，不能只以 PowerLock
  错误消失或自动化 Xorg/GL 探针通过作为成功判据。
- R10 随后在 `4.0.1-i3` 上稳定复现 deep PowerLock 故障，证明 patch-024 的锁外状态读取不能
  独立关闭竞态；当前修复方向和候选见 patch-026 生命周期同步说明。

## 参考

- [TI PowerVR Rogue 24.2 `pvr_dvfs_device.c`](https://github.com/TexasInstruments/ti-img-rogue-driver/blob/7cced54f276ac910dc67c6d8d9c9daa521995d04/services/server/env/linux/pvr_dvfs_device.c)
- [TI PowerVR 电源锁与 PreClock 语义](https://github.com/TexasInstruments/ti-img-rogue-driver/blob/7cced54f276ac910dc67c6d8d9c9daa521995d04/services/server/common/power.c)
- [Linux PowerVR suspend 同类竞态修复](https://github.com/torvalds/linux/commit/2d7f05cddf4c268cc36256a2476946041dbdd36d)
- [对应邮件讨论](https://patch.msgid.link/20260310-drain-irqs-before-suspend-v1-1-bf4f9ed68e75@imgtec.com)

这些材料说明 suspend 窗口必须阻止未同步的硬件访问，不是可直接 cherry-pick 的相同修复。
