# 故障恢复

## 显卡驱动回退

如果 `patched-17` 导致显示启动异常：

```sh
cd "$INNOGPU_ROOT"
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

先恢复可见 TTY，再处理 Xorg。不要在屏幕全黑时连续尝试新的 modeset。

## 显示 watcher 回退

显示 watcher 异常不等于 DKMS 异常。先停止新的 watcher，确保没有两条自动 RandR 写入链路，再恢复
上一版 `xdisplay.sh` 或使用 `displayselect` 建立可见单屏。必要时通过 SSH/TTY 操作。

## 音频恢复

若 ALSA 直连有声、应用无声，检查 PipeWire 默认 sink 和是否重新出现全局 `ALSA_CONFIG_PATH`。
重新应用项目配置：

```sh
sudo scripts/install-hygon-hda-audio.sh
```

该操作不需要重装显卡 DKMS。
