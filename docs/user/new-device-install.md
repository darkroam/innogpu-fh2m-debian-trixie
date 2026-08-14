# 新 Debian 设备安装

## 版本选择

当前有两个不同用途的入口：

- `patched-17` 是保守的自动安装入口，也是 patched-20 的当前回退点；
- `patched-20` 已在本机完成 PVR、Xorg/GLX、fbdev 和真实 VT 验收，但包含高频 PVR 诊断，仍是
  受控诊断候选，不能作为新设备默认长期版本。

`patched-8` 只保留为 patched-17 失败后的更早历史回滚物。patched-18/19 是问题定位和 coherent
构建演进记录，不是安装推荐版本。

## 准备文件

clone 本仓库后，从 release 下载需要的外部包到 `debs/`：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-20.deb
debs/innogpu-fh2m_20250421190503-debug_amd64.deb
```

这些文件和本地构建出的 deb 均被 `.gitignore` 忽略。`third_party/` 由脚本从 Deepin 原包重建，
也不进入 Git。

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
```

## 显示引擎前置条件

xdisplay 的唯一源码权威是 dotconfig。本仓库不携带 `xdisplay`、`displayselect`、共享库或引擎测试。
需要自动显示布局时，应先从 dotconfig 安装以下用户组件：

```text
~/.local/bin/xdisplay
~/.local/bin/xdisplay.sh
~/.local/bin/displayselect
~/.local/lib/xdisplay/
```

驱动安装不依赖 xdisplay。缺少上述组件时，patched-17 安装器只警告并跳过显示接入；安装 dotconfig
后可单独执行：

```sh
sudo INNOGPU_X_USER="$USER" INNOGPU_X_HOME="$HOME" \
  scripts/install-xdisplay-user.sh
```

该命令只安装 Innogpu 模式恢复钩子和 X11 会话接入，不会覆盖 dotconfig 的显示引擎。

## 保守入口：patched-17

先确认 patched-8 回退包已经放入 `debs/`，再执行：

```sh
cd "$INNOGPU_ROOT"
sudo scripts/install-patched17-and-check.sh
```

目标桌面用户不是 `SUDO_USER` 时，显式指定用户和主目录：

```sh
sudo INNOGPU_X_USER="$USER" INNOGPU_X_HOME="$HOME" \
  scripts/install-patched17-and-check.sh
```

安装器保持软件 Xorg 用户态，并准备 DKMS、模块自加载和 initramfs。重启后先验证 TTY、驱动节点和
软件 Xorg；需要复现历史硬件 GL 流程时再按 `verification.md` 分步执行。patched-17 的已知边界是
`/dev/fb0 mmap()` 返回 `ENODEV`，因此真实 VT fbterm 会失败。

## 受控候选：patched-20

只在已准备 patched-17 回退包、能够通过 SSH/TTY 恢复，并已阅读 `recovery.md` 后安装。使用 release
包时不需要本地重建；需要复现当前诊断候选时，必须从完整 Deepin 202504 原包构建：

```sh
cd "$INNOGPU_ROOT"
scripts/build-patched20-deepin-diagnostic.sh
```

不得以 patched-8、17、18 或 19 的 deb 作为载荷基线，也不得从不同版本挑选 DRI、GBM、GLAPI、
DDX 或固件文件拼装。受控安装步骤为：

```sh
sudo dpkg -i "$INNOGPU_ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-20.deb"
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```

重启后按 `verification.md` 依次检查包/DKMS、PVR services、DRM/fbdev、隔离 Xorg/GLX、正常桌面和
真实 VT fbterm。当前 `patch-008` 会重复写 PVR 诊断日志；该行为是保留 patched-20 为诊断候选、
不将其推广为长期版本的直接原因。

## 历史失败边界

- patched-18 虽修复 fbdev mmap，但曾以 patched-8 作为其余载荷基线，形成用户态 ABI 混配；同名包
  不应重建或部署。
- patched-18 的另一次试验缺少 `fh2m.sh` shader 固件，导致 PVR services 进入 bad state；不要把
  Xorg 中的 GBM 崩溃误判为单一 `.so` 问题。
- patched-19 首次恢复了完整 Deepin 202504 coherent 载荷，只保留为 patched-20 的构建演进记录。

详细证据见 [`docs/incidents/`](../incidents/README.md) 和
[`docs/patches/`](../patches/README.md)。

## 内置喇叭

```sh
sudo scripts/install-hygon-hda-audio.sh
```

需要测试音时使用 `sudo scripts/install-hygon-hda-audio.sh --test-sound`。音频恢复不要求重装
Innogpu DKMS。

## 可选 Picom

显卡驱动、Xorg 和硬件 GL 验证完成后，再按 [`picom-install.md`](picom-install.md) 安装 patched
Picom。不要在首次驱动启动验证前同时启用合成器。
