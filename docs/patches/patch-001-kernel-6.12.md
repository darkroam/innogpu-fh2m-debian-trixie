# Patch 001：Debian 6.12 兼容层

## 目的

让 Deepin 202504 DKMS 源码能够在 Debian Trixie 的 6.12 内核 headers 下编译和注册。

## 实现

- 代码：`patches/001-kernel-6.12-compat.patch`。
- 应用位置：`scripts/build-deepin-coherent.sh`，始终应用。
- 载荷边界：只修改 DKMS 源码，不替换 Deepin 用户态 DRI、GBM、GLAPI、GLVND 或 DDX。
- 兼容范围：Debian `6.12.101` 起 `pci_resize_resource()` 增加第四个
  `exclude_bars` 参数；补丁按内核版本传入 `0`，旧的 6.12 内核继续使用三参数接口。

## 验证

- DKMS 编译和模块安装通过。
- Deepin 原始 DKMS 源码叠加本补丁后，已在 `6.12.101+deb13-amd64` headers 离线编译通过；
  编译产生的 warning 来自厂商代码既有的 remove 回调和 objtool，不影响本次 API 修复。
- `card0`、`renderD128`、`fb0` 节点可创建。
- 后续补丁和运行时验证均以此为前置条件。

## 回退

构建失败时停止，不通过复制历史包中的 `.ko` 绕过。patched-24 已在
`6.12.101+deb13-amd64` 上完成离线 DKMS 编译和重启后的模块验证；旧 patched-23
仍只代表 `6.12.96` 的历史运行状态。运行时失败仍使用已验证的 patched-17 回退包。
