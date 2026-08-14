# 状态验证

## 驱动、DKMS 与节点

当前本机验收目标是 patched-20：

```sh
scripts/verify-install-status.sh 3.3.3.42-patched-20
cat /proc/driver/innogpu/gpu00/status
ls -l /dev/dri /dev/fb0
```

验证 patched-17 回退时把期望版本改为 `3.3.3.42-patched-17`。期望 Driver/Firmware 为 OK，并存在
`card0`、`renderD128` 和 `fb0`；版本不匹配时不能用当前模块结果证明新包通过。

## Xorg 与硬件 GL

```sh
scripts/check-innogpu-progress.sh
scripts/check-desktop-hwgl.sh
sudo scripts/check-post-reboot-hwgl.sh
sudo scripts/test-current-xorg-hwgl-runtime.sh
```

patched-20 的验收标志包括 `PASS_DESKTOP_HWGL`、`PASS_POST_REBOOT_HWGL` 和
`PASS_CURRENT_XORG_HWGL_RUNTIME`。应确认 renderer 为 `Fantasy II-M`、direct rendering 启用，并有
DRI3、GLX、Present 和 RANDR。提交中的 baseline 只是历史证据，不能替代当前设备检查。

## framebuffer 与真实 VT

patched-20 必须在真实 VT 验证 fbdev mmap 和 fbterm。通过 SSH 保存内核日志后，切换到未运行 Xorg
的真实 VT 执行：

```sh
fbterm
```

普通用户出现“cannot change kernel keymap table”只表示 fbterm 快捷键受限；能够进入、绘制并正常
退出即通过。段错误时使用 `strace -ff -o "$HOME/fbterm.strace" fbterm` 保存本机证据，但不要将
trace、用户名或绝对 home 路径提交到仓库。

## 显示管理接入

xdisplay 由 dotconfig 维护。在 X11 会话中使用 dotconfig 的命令只读检查：

```sh
xdisplay status
xrandr --current
cat /sys/class/tty/tty0/active
```

实机验证必须同时看状态输出、RandR 和物理屏幕，不能只看 `connected`。xdisplay 的状态机、适配器、
多外屏、配置和自定义布局测试在 dotconfig 运行：

```text
.local/share/test/display/xdisplay-adapter.sh
```

Innogpu 仓库只运行不会启动 watcher、不会改变布局的接入测试：

```sh
tests/xdisplay/run-install-tests.sh
```

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
