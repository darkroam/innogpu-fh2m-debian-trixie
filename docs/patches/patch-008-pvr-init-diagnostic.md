# Patch 008：PVR 初始化诊断

## 目的

记录 `DRM_IOCTL_PVR_SRVKM_INIT` 前后的模块号、设备状态和 services 返回码，用于区分固件缺失、
设备 BAD 状态和用户态 ABI 问题。

## 实现

- 代码：`patches/008-pvr-init-diagnostic.patch`。
- 开关：`APPLY_PVR_INIT_DIAGNOSTIC=1`。
- 历史入口：`scripts/build-patched20-deepin-diagnostic.sh`；当前仅保留为拒绝同版本重建的护栏。
- 不改变 services 调用参数、错误映射或用户态库。

## 验证

patched-20 日志显示：

```text
GPU00 Firmware image 'innogpu/fh2m.fw' loaded
GPU00 Shader binary image 'innogpu/fh2m.sh' loaded
srvkm_init state_before=2 ret=0 state_after=3
```

这确认 shader 固件完整后 PVR 可进入 `PVRSRV_DEVICE_STATE_ACTIVE`。

## 生命周期

这是诊断候选，不是长期日志方案。当前每次 ioctl 都会记录，已观察到短时间内大量重复条目；
下一长期版本应移除、限速或改为只记录首次失败。
