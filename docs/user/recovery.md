# 故障恢复

## 恢复顺序

驱动回退顺序固定为：

```text
当前链：patched-27 -> patched-26 -> patched-25 -> patched-24 -> patched-23 -> patched-22
历史链：patched-22 -> patched-21 -> patched-17 -> patched-8
```

patched-17 是当前保守回退点，patched-8 只在 patched-17 仍不能启动时使用。执行任何升级前都应把
这两个 deb 放入 `debs/`，并保留 SSH 或真实 TTY 恢复通道。

本页可以独立使用。先在可用 SSH 或 TTY 中设置仓库路径；仓库不在默认位置时，先把变量改为实际 clone
目录。若检查失败，不要继续执行包替换命令：

```sh
export INNOGPU_ROOT="${INNOGPU_ROOT:-$HOME/src/innogpu-fh2m-debian-trixie}"
test -x "$INNOGPU_ROOT/scripts/verify-install-status.sh" || {
  printf '%s\n' "INNOGPU_ROOT is not an innogpu repository: $INNOGPU_ROOT" >&2
  exit 1
}
cd "$INNOGPU_ROOT"
```

## patched-22 回退到 patched-21

```sh
sudo dpkg -i "$INNOGPU_ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb"
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```

重启后使用 `scripts/verify-install-status.sh --require-reboot 3.3.3.42-patched-21` 验证稳定图形基线。
必须整体切回 p21，禁止只替换一个 vendor `.so`。

## patched-21 回退到 patched-17

```sh
sudo dpkg -i "$INNOGPU_ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb"
sudo scripts/disable-incompatible-userspace.sh
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```

必须整体切回 patched-17 的包和软件 Xorg 路径，禁止只替换一个 vendor `.so`。重启后使用
`scripts/verify-install-status.sh --require-reboot 3.3.3.42-patched-17` 验证实际运行版本。

## patched-17 回退到 patched-8

```sh
sudo dpkg -i "$INNOGPU_ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb"
sudo scripts/disable-incompatible-userspace.sh
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```

## 恢复 TTY 或软件 Xorg

```sh
sudo scripts/restore-tty1-login.sh
sudo scripts/prepare-soft-xorg-dwm.sh
```

先恢复可见 TTY，再处理 Xorg。不要在屏幕全黑时连续尝试新的 modeset。若临时 Xorg `:9` 未退出，
先执行 `sudo pkill -f 'Xorg :9'`，必要时再 `sudo chvt 1`。

## 显示接入恢复

xdisplay watcher 异常不等于 DKMS 异常。先停止重复 watcher，确保只有一条 RandR 写入链路，再在
dotconfig 仓库恢复或验证 xdisplay；不要从本仓库寻找或安装引擎副本。Innogpu 只负责以下接入：

```text
~/.local/bin/innogpu-restore-dp1-mode-x11
~/.config/x11/innogpu-display-session.sh
XDISPLAY_INTERNAL_OUTPUTS="eDP-1 DP-1"
XDISPLAY_RESTORE_COMMAND=innogpu-restore-dp1-mode-x11
```

必要时可先停止 watcher，用 dotconfig 的 `displayselect` 建立可见单屏，再检查上述钩子。接入契约见
[`../project/display-management.md`](../project/display-management.md)。

## 音频恢复

若 ALSA 直连有声、应用无声，检查 PipeWire 默认 sink 和是否重新出现全局 `ALSA_CONFIG_PATH`：

```sh
sudo scripts/install-hygon-hda-audio.sh
```

该操作不需要重装显卡 DKMS。
