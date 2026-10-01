# 030-035：PVR 私有对象 accessor 修复

## 问题

`030-026-lifecycle` 将 Deepin 的 DVFS suspend/resume 接线移植到 F 驱动时，把
`innogpu_drm_to_pvr_private(ddev)` 适配成了直接读取 `ddev->dev_private`。F 的该字段实际指向
外层 `struct fantgpu_drm_private`，而不是内嵌的 `struct ft_drm_private`。错误布局读取使
`priv->dev_node` 为 NULL，`SuspendDVFS()` 返回 `PVRSRV_ERROR_INVALID_DEVICE(6)`，最终成为
`-EFAULT`；异步恢复阶段的父设备等待是这次 suspend 失败的后果。

## 修复

`patches/030-035.patch` 只在 `ft_pm_suspend()` 与 `ft_pm_resume()` 中复用已有
`fantgpu_drm_to_ft_private(ddev)`。不修改 `SuspendDVFS()`、`ResumeDVFS()`、devfreq 注册、结构布局、
闭源对象或 PM 顺序。R48 定位期间加入的未注册 devfreq guard 属错误假设下的临时改动，不进入本补丁。

邻接审查发现主 DRM `postclose` 的 `fantgpu_ft_drm_release()` 仍有同类直接类型解释。当前 shipped
`PVRSRVDeviceRelease()` 反汇编确认未读取第一个 `psDeviceNode` 参数，故它不参与本次 DVFS 故障且当前
无运行影响；该生命周期清理须单独审查，不扩入本补丁。

补丁以锁定的 i6/`030-034` 树为父树，严格零 fuzz 应用后的树哈希为
`995a77af4fd25c29746aee2441256d6af1b96ca4b346059c8d3ed663a484b98b`。i8–i11 派生不修改
`ft_drm.c`，因此可在派生前后等价应用；历史 i6 快照保持不可变。

## 证据与边界

R48 netconsole 捕获了 `dvfs_suspend rc=6 propagated=-14`，反汇编证明失败入参为 NULL；修复后的
r48 模块在 `pm_async=1` 下连续三次 `pm_test=devices` 和一次正常 deep suspend/resume 均返回
`rc=0`。该模块仍含诊断内核与临时 guard，因此只作为根因支持证据；正式干净候选须在审查后独立构建、
安装并重复相同运行判据。

静态回归入口：

```bash
bash tests/unit/run-030-035-private-accessor-tests.sh
```

回滚方式是丢弃派生树并重新解包锁定 i6 快照，不反向修改历史快照或旧补丁。
