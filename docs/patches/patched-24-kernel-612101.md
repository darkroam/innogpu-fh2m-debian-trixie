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

## 验证要求

构建后必须运行：

```sh
scripts/check-release-package.sh debs/innogpu-fh2m-trixie_3.3.3.42-patched-24.deb
scripts/check-deb-dkms-build.sh debs/innogpu-fh2m-trixie_3.3.3.42-patched-24.deb 6.12.101+deb13-amd64
```

安装 patched-24 后，`dpkg --configure -a` 应能完成；是否重启由维护者在确认回退包和
新模块安装成功后单独决定。本次构建不自动安装、不热切换、不自动重启。
