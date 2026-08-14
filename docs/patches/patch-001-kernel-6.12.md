# Patch 001：Debian 6.12 兼容层

## 目的

让 Deepin 202504 DKMS 源码能够在 Debian Trixie 的 6.12 内核 headers 下编译和注册。

## 实现

- 代码：`patches/001-kernel-6.12-compat.patch`。
- 应用位置：`scripts/build-deepin-coherent.sh`，始终应用。
- 载荷边界：只修改 DKMS 源码，不替换 Deepin 用户态 DRI、GBM、GLAPI、GLVND 或 DDX。

## 验证

- DKMS 编译和模块安装通过。
- `card0`、`renderD128`、`fb0` 节点可创建。
- 后续补丁和运行时验证均以此为前置条件。

## 回退

构建失败时停止，不通过复制历史包中的 `.ko` 绕过。运行时使用已验证的 patched-17 回退包。
