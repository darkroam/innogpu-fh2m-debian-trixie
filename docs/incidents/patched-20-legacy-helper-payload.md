# patched-20：运行验收物含旧辅助载荷

## 发现

2026-08-14 第三轮仓库审计确认，本机已安装并完成运行验收的 patched-20 deb 生成于 xdisplay
所有权收敛之前。其 SHA-256 为：

```text
747380492876d73fa72b5483ff0508ca0bd0a382efbc6c315fd6a6bf58ad23b6
```

只读包清单显示它仍包含：

```text
usr/share/innogpu-fh2m-trixie/xdisplay.sh
usr/share/innogpu-fh2m-trixie/displayselect
usr/share/innogpu-fh2m-trixie/install-kylin-userspace.sh
usr/share/innogpu-fh2m-trixie/install-experimental-hwgl.sh
usr/share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh
```

包内旧 `install-xdisplay-user.sh` 会复制旧引擎；由包提供的 `innogpu-prepare-soft-xorg-dwm` 可能间接
调用它。当前 dotconfig xdisplay 与仓库接入脚本均已演进，不能把这些已安装文件视为当前源码。

## 影响边界

- patched-20 的 PVR、Xorg/GLX、fbdev 和真实 VT 运行验收仍成立；问题位于发布辅助载荷，不是驱动
  运行时回归。
- 该 deb 不得上传为当前 release，也不得部署到新设备。
- 升级到后续合规包前，不调用包内旧显示安装/恢复入口；需要恢复时使用当前仓库脚本和 dotconfig
  权威 xdisplay。
- 当前源码删除了这些文件，因此再以 `patched-20` 版本号构建会生成内容不同的同名包，破坏版本与
  证据对应关系。

## 修正与后续门槛

1. `build-patched19-deepin-coherent.sh` 和 `build-patched20-deepin-diagnostic.sh` 只保留为拒绝执行的
   历史兼容入口。
2. `build-deepin-coherent.sh` 只接受大于 20 的显式新版本号。
3. `check-release-package.sh` 拒绝 xdisplay 引擎副本、历史实验安装器、直接二进制热补丁命令和与
   当前源码不一致的接入脚本。
4. 下一候选必须重新完成包清单、DKMS、固件、PVR、Xorg/GLX、桌面和真实 VT 验收，不能继承 p20
   的运行结论作为新包证据。
