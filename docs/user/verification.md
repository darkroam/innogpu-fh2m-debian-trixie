# 状态验证

## 驱动与节点

```sh
scripts/verify-install-status.sh 3.3.3.42-patched-17
cat /proc/driver/innogpu/gpu00/status
ls -l /dev/dri /dev/fb0
```

期望 Driver/Firmware 为 OK，并存在 `card0`、`renderD128` 和 `fb0`。

## Xorg 与硬件 GL

```sh
scripts/check-innogpu-progress.sh
scripts/check-desktop-hwgl.sh
sudo scripts/check-post-reboot-hwgl.sh
```

历史成功标志是 `PASS_DESKTOP_HWGL` 和 `PASS_POST_REBOOT_HWGL`。提交中的 baseline 只是历史证据，
不能替代当前设备检查。

## 显示管理

代码吸纳完成后使用：

```sh
xdisplay.sh --status
xrandr --current
cat /sys/class/tty/tty0/active
```

`--status` 应只读，并报告 lid、输出 connection/geometry/mode、stale/pending、health 和锁路径。
实机验证必须同时看 RandR 状态和物理屏幕，不能只看 `connected`。

## 内置音频

```sh
lspci -nnk -s 06:00.6
aplay -l
amixer -c Intel sget Speaker
wpctl status
systemctl status hygon-hda-audio.service --no-pager
systemctl --user status hygon-hda-audio-user.service --no-pager
```

期望 HDA 使用 `snd_hda_intel`，ALSA 出现 `SN6180 Analog`，Speaker 为 `[on]`，PipeWire 默认 sink
是 `HDA Intel 模拟立体声`。
