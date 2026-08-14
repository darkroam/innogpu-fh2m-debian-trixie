# patched-18：shader 固件缺失导致 PVR BAD

## 现象

同源 DRI/GLAPI 替换后，临时 Xorg 仍在 `gbm_create_device()` 处崩溃，services 初始化返回 `ENODEV`。
内核日志随后重复报告设备已处于 BAD 状态。

## 根因链

失败日志明确显示：主固件 `fh2m.fw` 成功加载，但 `fh2m.sh`、版本命名的 shader 文件和对应请求
均返回 `-2`；`PVRSRVTQLoadShaders` 报 `NOT_FOUND`，进而 `RGXInitDevPart2`、`RGXInit` 和
`PVRSRVCommonDeviceInitialise` 失败，设备进入 `PVRSRV_DEVICE_STATE_BAD`，后续 services open
映射为 `ENODEV`。DMA/IOMMU 告警、zink 回退和 GBM 段错误是无关或下游现象，不能作为第一根因。

## 修复

Deepin 202504 原包中的 `fh2m.fw`、`fh2m.sh`、`fh2c.fw`、`fh2c.sh` 必须作为完整集合保留。禁止
从旧包复制单个固件或用户态库；缺失的 `hwinfo_g0m.bin` 是独立告警，在没有状态证据前不能替代
shader 缺失作为根因。

## 经验

当 services 返回 `ENODEV` 时，先记录固件请求、RGX 初始化结果和 device state，再判断用户态、内核
或硬件配置。不要仅凭错误码猜测，也不要用 Xorg、Picom 或显示布局配置掩盖 PVR 初始化故障。
