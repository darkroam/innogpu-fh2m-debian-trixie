# 新 Debian 设备安装

## 准备文件

clone 本仓库后，从 release 下载以下文件到 `debs/`：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-18.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-20.deb
debs/innogpu-fh2m_20250421190503-debug_amd64.deb
```

这些文件被 `.gitignore` 忽略。`third_party/` 由脚本从 Deepin deb 重建，也不进入 git。

## 安装依赖

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
```

## 安装 patched-17 回退包

```sh
cd "$INNOGPU_ROOT"
sudo scripts/install-patched17-and-check.sh
```

该脚本仅用于安装 patched-17 回退包；当前已验收版本为 patched-20。脚本还会为执行 `sudo` 的桌面用户安装显示命令和 X11 会话启动入口，但不会在安装过程中启动
watcher 或改变当前 RandR 布局。使用 root 直接安装或目标桌面用户不是 `SUDO_USER` 时应显式
传入目标用户，例如：

```sh
sudo INNOGPU_X_USER="$USER" INNOGPU_X_HOME="$HOME" scripts/install-patched17-and-check.sh
```

需要重启时，先阅读 `recovery.md` 并确认 `debs/` 中有 `patched-17` deb。重启后先验证 TTY、Xorg、
dwm 和基础驱动状态，再启用 Deepin 硬件 GL 用户态。`patched-8` 只保留为更早的历史回滚物。

```sh
scripts/verify-install-status.sh 3.3.3.42-patched-17
scripts/prepare-deepin-userspace-root.sh
sudo scripts/run-local-ddx-vt-test.sh
sudo scripts/install-deepin-desktop-hwgl-trial.sh
```

硬件 GL 和显示切换的完整验证见 `verification.md`。

## patched-18/19 历史问题与 patched-20 验收结果

`patched-17` 的 `/dev/fb0` ioctl 可用，但 framebuffer `mmap()` 返回 `ENODEV`，会导致 fbterm
在真实 VT 中段错误。`patched-18` 只增加 fbdev I/O mmap 回调，保留 patched-17 作为回退点。

patched-18 的历史构建入口已经停用，因为它虽然替换了 Deepin 202504 DKMS 源码，却以 patched-8
`.deb` 作为其余载荷和控制脚本基线，会形成不完整的用户态 ABI 集合。不要尝试重建同名 patched-18。

历史候选从完整 Deepin 202504 原包构建：

```sh
cd "$INNOGPU_ROOT"
scripts/build-patched19-deepin-coherent.sh
```

该入口整体保留 Deepin DRI/GBM/GLAPI/GLVND/DDX，只在 DKMS 源码和已审查的 Debian 安装逻辑上
叠加修改，并自动拒绝关键 vendor 文件变化或 DRI 未解析符号。构建成功不等于可安装发布；必须先按
项目文档完成隔离 Xorg 验证。

patched-19 的安装步骤仅用于历史复现，不是当前推荐版本；当前已验证版本为 patched-20。安装驱动
需要 root 并会触发 DKMS 重建，不要在重要 X11 工作进行时热卸载当前模块：

```sh
sudo dpkg -i "$INNOGPU_ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-19.deb"
/sbin/dkms status innogpu-kernel
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```

安装后先运行隔离 Xorg/GLX；通过后再重启。重启后先运行 `/tmp/fbprobe` 或等价探针，预期
`mmap()` 成功，再验证正常桌面，最后从真实 VT 启动 `fbterm`。若
失败，使用保存的 patched-17 包回退并重启：

```sh
sudo dpkg -i "$INNOGPU_ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb"
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```

patched-19 仅保留为完整载荷构建记录。patched-20 已在本机安装并成功重启，PVR shader 固件、
services 初始化、隔离 Xorg/GLX、framebuffer `mmap()` 和真实 VT `fbterm` 均已通过；普通用户运行
`fbterm` 时仅有无法修改内核键盘映射的非致命警告。

> **patched-18 当前候选包注意事项：** 首次实机安装已确认 fbdev `mmap()` 成功，但包内现有
> `postinst` 会把活动的 `innogpu_dri.so` 移为 `.bak`。在已经启用硬件 GL 的主机上，这会破坏
> Xorg 的完整用户态集合；若 `.bak` 与当前 `libglapi_inno` 来自不同版本，直接移回仍会因未解析
> `_glapi_*` 符号导致 Xorg 段错误。修正安装流程前，不应在其他硬件 GL 主机上直接部署该候选包；
> 恢复时必须使用同一发行版本、符号契约一致的一整套 DRI/GBM/GLAPI/DDX 文件。

## 内置喇叭

```sh
sudo scripts/install-hygon-hda-audio.sh
```

该脚本会安装 HDA 驱动绑定和用户级 PipeWire 恢复服务。需要测试音时使用：

```sh
sudo scripts/install-hygon-hda-audio.sh --test-sound
```

## 显示会话

patched-17 安装脚本会安装显示命令和会话入口。首次启动 Xorg 前可先运行只读 fixture；进入 X11
后按 `verification.md` 检查 `xdisplay.sh --status`。项目不会复制完整个人 dotfiles，dwm 的键位、
状态栏、壁纸、输入法和代理仍由桌面配置自行提供。Picom 是下述独立可选流程。

## 可选 Picom GLX 合成器

显卡驱动、Xorg 和硬件 GL 验证完成后，再按 `picom-install.md` 安装 patched Picom。该流程独立
clone 固定 Picom 源码、应用项目 patch、安装配置和注册 X11 会话入口；不要把 Picom 构建目录或
二进制加入本仓库，也不要在首次显卡启动验证前同时启用合成器。
