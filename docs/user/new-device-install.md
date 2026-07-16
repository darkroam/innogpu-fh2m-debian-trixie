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

当前安装流程会准备 Xorg/dwm 和显示辅助工具。显示 watcher 新实现尚在吸纳阶段，具体状态以
`../project/display-management.md` 为准；在该文档改为“仓库已生效”前，不应假设新 clone 已包含
本机 dotfiles 的全部显示行为。
