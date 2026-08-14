# Patch 007：fbdev I/O mmap

## 目的

修复 patched-17 中 `/dev/fb0` 的 `mmap()` 返回 `ENODEV`，导致 fbterm 使用失败的问题。

## 实现

- 代码：`patches/007-fbdev-io-mmap.patch`。
- 开关：`APPLY_FBDEV_IO_MMAP=1`。
- 变更：为 `s_inno_fbdev_ops` 显式设置 `.fb_mmap = fb_io_mmap`，并引入 `linux/fb.h`。

## 证据

- 独立探针：`/dev/fb0` 参数 ioctl 成功，patched-17 的映射返回 `ENODEV`。
- patched-20：16,384,000 字节映射成功，`FBIOPAN_DISPLAY` 返回 `0`。
- 真实 VT：`fbterm rc=0`。

## 边界

该修复不负责 fbterm 键盘快捷键权限，也不应通过修改 fbterm 本身掩盖驱动映射失败。
