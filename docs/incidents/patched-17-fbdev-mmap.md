# patched-17：fbdev mmap 失败与 fbterm 崩溃

## 现象

在真实 VT 运行 `fbterm` 时进程收到 `SIGSEGV`。独立 framebuffer 探针能够打开 `/dev/fb0`，但
使用 `FBIOGET_FSCREENINFO.smem_len` 映射 framebuffer 返回 `ENODEV`。

## 证据与推导

`FBIOGET_FSCREENINFO`、`FBIOGET_VSCREENINFO` 成功而 `mmap()` 失败，说明设备节点和基本 ioctl
可用，故障位于 fbdev 映射回调。fbterm 旧版本在绘制路径未可靠检查 `MAP_FAILED`，因此映射失败
随后表现为段错误；从 SSH 或 X11 终端运行只能得到 tty 前置错误，不能代替真实 VT 验证。

## 修复边界

在 Deepin 202504 DKMS 源码的 `s_inno_fbdev_ops` 中显式设置 `.fb_mmap = fb_io_mmap`，形成
`patches/007-fbdev-io-mmap.patch`。这是驱动映射修复，不是 fbterm 权限或 Xorg 配置修复；普通用户
仍可能看到不能修改内核键盘表的非致命警告。

## 验收

patched-20 上探针映射 16,384,000 字节成功，`FBIOPAN_DISPLAY` 返回 0，真实 VT `fbterm` 正常
退出。后续版本必须从 Deepin 202504 完整载荷重新应用该补丁，不能从历史包复制单个 `.so`。
