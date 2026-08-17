# patched-24：Debian 6.12.101+ DKMS 兼容包

## 目的

修复 Debian `6.12.101` 及以后内核 headers 中 `pci_resize_resource()` 增加
`exclude_bars` 参数造成的 DKMS 构建失败。

## 构建边界

- 构建入口：`scripts/build-patched24-kernel-612101.sh`。
- 基线：Deepin 202504 原 deb。
- 行为基线：patched-23 的 patch-000、patch-001/002/006/007/009/023 开关集合。
- 新增兼容：patch-001 在 `6.12.101+` 传入 `exclude_bars=0`，旧内核仍走三参数接口。
- 用户态 DRI、GBM、GLAPI、GLVND、DDX、固件和显示行为不变。

## 构建验证

构建后必须运行：

```sh
scripts/check-release-package.sh debs/innogpu-fh2m-trixie_3.3.3.42-patched-24.deb
scripts/check-deb-dkms-build.sh debs/innogpu-fh2m-trixie_3.3.3.42-patched-24.deb 6.12.101+deb13-amd64
```

构建结果：

- release 包边界检查通过。
- `6.12.101+deb13-amd64` 离线 DKMS 编译通过，模块 vermagic 匹配。
- 包 SHA-256：`20ceccdcb507f80d2c41198046e037ce8fa6381f217c5861aad0dafdc4c01744`。

## 实机重启验证

2026-08-18 重启后确认：

- 安装版本为 `3.3.3.42-patched-24`，运行内核为 `6.12.101+deb13-amd64`。
- DKMS 同时存在 `6.12.101+deb13-amd64` 和旧 `6.12.96+deb13-amd64` 实例，p24 模块已自动加载。
- Driver Status、Firmware Status 均为 `OK`，错误计数为 0。
- `/dev/dri/card0`、`/dev/dri/renderD128`、`/dev/fb0` 均存在，boot autoload 已启用。
- `dpkg` 安装状态正常，未发生新的 DKMS 配置失败。

本次自动化检查运行在隔离会话中，无法读取真实 X display，因此没有把 Xorg/dwm
进程检查写成 p24 的新增运行证据；此前 p23/p21 的桌面验收仍作为行为基线。
