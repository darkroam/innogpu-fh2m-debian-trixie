# 音频管理

## 硬件关系

| 设备 | 驱动 | 用途 |
| --- | --- | --- |
| `InnosiliconCard` | `innogpu` | DP/HDMI 数字音频 |
| `0000:06:00.6 [1d94:14c9]` | `snd_hda_intel` | Conexant SN6180 内置模拟音频和喇叭 |
| USB Audio | `snd_usb_audio` | 扩展坞耳机和麦克风 |

Debian 内核包含 `snd_hda_intel`，但其 PCI alias 不自动匹配 `1d94:14c9`。本项目通过
`scripts/install-hygon-hda-audio.sh` 安装系统级绑定服务和用户级 PipeWire 恢复服务。

## 持久化文件

```text
/etc/modules-load.d/hygon-hda-audio.conf
/etc/systemd/system/hygon-hda-audio.service
~/.config/systemd/user/hygon-hda-audio-user.service
~/.local/bin/hygon-hda-audio-user-apply
~/.config/alsa/asoundrc
~/.asoundrc -> ~/.config/alsa/asoundrc
```

系统服务在启动时加载并绑定 HDA。用户服务在 PipeWire/WirePlumber 启动后设置 HDA Intel 为默认
sink、取消静音并恢复 mixer，避免 WirePlumber route 恢复把 Speaker 重新关闭。

## PipeWire 约束

PipeWire、pipewire-pulse 和 WirePlumber 由 systemd 用户服务负责。不要在 `xprofile` 或自定义
`pipewire.conf` 中再次启动它们。

不要全局导出：

```sh
ALSA_CONFIG_PATH="$XDG_CONFIG_HOME/alsa/asoundrc"
```

该变量会替换 `/usr/share/alsa/alsa.conf`，导致 WirePlumber 只能看到 `off` profile 或虚拟输出。
用户 ALSA 默认值应通过标准 `~/.asoundrc` 链接加载。

## 验证

当前状态不能只看 mixer。必须同时确认 PCI 驱动、ALSA 设备、Speaker switch、PipeWire sink 和
WirePlumber 默认节点。完整命令见 `../user/verification.md`。
