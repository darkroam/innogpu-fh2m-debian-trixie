# patched-18：用户态 ABI 混配

## 现象

patched-18 的内核模块和 framebuffer 修复可构建，但隔离 Xorg 在 GBM 初始化阶段退出 134。日志
先出现 `innogpu_dri.so: undefined symbol: _glapi_tls_Dispatch`，随后 GBM 清理路径发生段错误。

## 根因

历史构建器只替换了 Deepin 202504 DKMS 源码，却以 patched-8 包作为用户态载荷和 maintainer
scripts 基线，造成旧 DRI 与新 `libglapi_inno` 的私有符号契约不一致。这个问题不是新增
`fb_io_mmap` 代码引起的，也不能通过再补一个 `.so` 稳定解决。

## 不可变规则

后续候选包必须直接解包同一份 Deepin 202504 原包，整体保留 DRI、GBM、GLAPI、GLVND、Xorg DDX
和固件，再只对 DKMS 源码应用仓库补丁。构建后必须核对关键文件与原包一致，并执行 `ldd -r` 检查
未解析符号；patched-8、patched-17、patched-18、patched-19 只能作为历史证据或回退物。

## 结果

`patched-19` 首次按完整载荷规则构建，`patched-20` 在此基础上完成运行时验收，旧 ABI 混配路径
不再作为修复方向。
