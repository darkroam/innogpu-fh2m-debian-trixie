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

当前新架构候选版本是 `4.0.1-i1`，固定构建 epoch 为 `1788278400`（2026-09-02 00:00 +0800）。
`scripts/build-innogpu-driver.sh` 对其他版本或 epoch 失败关闭，并把 patch-024 分别应用到离线编译
staging 与最终包内 DKMS 源码，避免编译内容和安装后 DKMS 重建内容不一致。

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
- 新架构构建器固定 `4.0.1-i1`/epoch，且编译树和包内源码都应用 patch-024；
- coherent 构建器与 patched-28 legacy 包装器的开关、版本和 p27 继承关系完整。

这只证明补丁可应用和接线正确，不证明真机恢复问题已经解决。

## 真机验证计划

构建需要本地 Deepin 原包，属于本轮禁止读取的 `debs/`/本地载荷边界，因此 `4.0.1-i1` 构建与
DKMS 安装尚未执行。获得载荷访问授权后按以下顺序执行，每一步先保存输出和 journal cursor：

1. 记录 `uname -r`、当前包版本、`cat /sys/power/mem_sleep`、当前启动时间和
   `journalctl -k --show-cursor`；保留当前 4.0.0-i1 及 patched-27 回退包和恢复命令。
2. 构建 `4.0.1-i1`，先执行包边界与离线 DKMS 编译检查；保存候选 SHA-256。
3. 安装后重启，不热切换活动模块；运行
   `scripts/verify-install-status.sh --require-reboot 4.0.1-i1` 和基础图形检查。
4. 设为 `deep`，执行一次合盖挂起/唤醒。通过判据：内屏恢复，SSH 和真实 TTY 可用；新增 journal
   中不存在 `3900372`、`PVRSRVEPowerLock failed` 或新的 PVR/Firmware 错误；驱动状态及错误计数
   不退化。
5. s2idle 必须使用经单独复审、能在真实 suspend entry 时保持目标模式的方案，并由外部观察者核对
   journal 确为 `PM: suspend entry (s2idle)`；禁止再次使用“`systemctl suspend` 返回后 trap 恢复
   deep”的错误方法。只有实际通过才能把 s2idle 记为短期规避。

若 `4.0.1-i1` 启动或恢复失败，优先回退当前已验证的 `4.0.0-i1`，再按既有链降级 patched-27；
不要在活动图形会话中热卸载模块。

## 当前结论

- deep：`4.0.0-i1` 于 2026-09-02 再次复现同一时序及红屏/黑屏/网络失效；`4.0.1-i1` 候选修复
  已实现但未运行。
- s2idle：2026-09-02 的尝试无效，脚本在真正挂起前恢复了 deep，journal 明确记录 S3；因此
  **尚无 s2idle 实测证据，当前不能作为已确认规避方案**。详见
  [事故记录](../incidents/suspend-resume-deep-reproduction-20260902.md)。

## 参考

- [TI PowerVR Rogue 24.2 `pvr_dvfs_device.c`](https://github.com/TexasInstruments/ti-img-rogue-driver/blob/7cced54f276ac910dc67c6d8d9c9daa521995d04/services/server/env/linux/pvr_dvfs_device.c)
- [TI PowerVR 电源锁与 PreClock 语义](https://github.com/TexasInstruments/ti-img-rogue-driver/blob/7cced54f276ac910dc67c6d8d9c9daa521995d04/services/server/common/power.c)
- [Linux PowerVR suspend 同类竞态修复](https://github.com/torvalds/linux/commit/2d7f05cddf4c268cc36256a2476946041dbdd36d)
- [对应邮件讨论](https://patch.msgid.link/20260310-drain-irqs-before-suspend-v1-1-bf4f9ed68e75@imgtec.com)

这些材料说明 suspend 窗口必须阻止未同步的硬件访问，不是可直接 cherry-pick 的相同修复。
