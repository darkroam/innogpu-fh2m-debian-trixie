# patched-20：完整载荷运行时验收

## 验收范围

`3.3.3.42-patched-20` 从 Deepin 202504 完整原包构建，保留 fbdev I/O mmap 修复并临时加入
PVR services 初始化诊断。诊断只记录 ioctl 前后状态和返回码，不改变调用参数或错误映射。

## 结果

- `fh2m.fw` 和 `fh2m.sh` 均加载，`srvkm_init state_before=2 ret=0 state_after=3`，设备进入 ACTIVE。
- 隔离 Xorg `:9/vt8`、`xdpyinfo` 和 `glxinfo` 均通过，direct rendering 和硬件加速启用。
- 真实 VT 的 fbterm 映射和 `FBIOPAN_DISPLAY` 通过，普通用户退出码为 0。

## 当前边界

PVR 诊断会重复写内核日志，属于诊断候选而非长期发布配置。正式版本应移除、限速或只记录首次
失败，然后重复 PVR、Xorg/GLX、正常桌面和真实 VT 全部门槛。原始日志放在用户本机；仓库只保留
`baselines/latest-current-xorg-hwgl-test/result.txt` 等精简证据。
