# 新 Debian 设备安装

## 准备文件

clone 本仓库后，从 release 下载以下文件到仓库根目录：

```text
innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
innogpu-fh2m_20250421190503-debug_amd64.deb
```

这些文件被 `.gitignore` 忽略。`third_party/` 由脚本从 Deepin deb 重建，也不进入 git。

## 安装依赖

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
```

## 安装 patched-17

```sh
cd "$INNOGPU_ROOT"
sudo scripts/install-patched17-and-check.sh
```

该脚本还会为执行 `sudo` 的桌面用户安装显示命令和 X11 会话启动入口，但不会在安装过程中启动
watcher 或改变当前 RandR 布局。使用 root 直接安装或目标桌面用户不是 `SUDO_USER` 时应显式
传入目标用户，例如：

```sh
sudo INNOGPU_X_USER="$USER" INNOGPU_X_HOME="$HOME" scripts/install-patched17-and-check.sh
```

需要重启时，先阅读 `recovery.md` 并确认 `patched-8` deb 在仓库根目录。重启后先验证 TTY、Xorg、
dwm 和基础驱动状态，再启用 Deepin 硬件 GL 用户态。

```sh
scripts/verify-install-status.sh 3.3.3.42-patched-17
scripts/prepare-deepin-userspace-root.sh
sudo scripts/run-local-ddx-vt-test.sh
sudo scripts/install-deepin-desktop-hwgl-trial.sh
```

硬件 GL 和显示切换的完整验证见 `verification.md`。

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
状态栏、壁纸、Picom、输入法和代理仍由桌面配置自行提供。
