# 音频管理

## 硬件关系

| 设备 | 驱动 | 用途 |
| --- | --- | --- |
| `InnosiliconCard` | `innogpu` | DP/HDMI 数字音频 |
| `0000:06:00.6 [1d94:14c9]` | `snd_hda_intel` | Conexant SN6180 内置模拟音频和喇叭 |
| USB Audio | `snd_usb_audio` | 扩展坞耳机和麦克风 |

Debian 内核包含 `snd_hda_intel`，但其 PCI alias 不自动匹配 `1d94:14c9`。本项目通过
`tools/debian/install-hygon-hda-audio.sh` 安装系统级绑定服务和用户级 PipeWire 恢复服务。

## 持久化文件

```text
/etc/modules-load.d/hygon-hda-audio.conf
/etc/systemd/system/hygon-hda-audio.service
~/.config/systemd/user/hygon-hda-audio-user.service
~/.local/bin/hygon-hda-audio-user-apply
~/.config/alsa/asoundrc
~/.asoundrc -> ~/.config/alsa/asoundrc
```

若对应文件已存在且内容不同，安装器先 `cp -a` 到同目录的 `.bak-hygon-hda-<时间戳>`。
备份失败则不覆盖并退出。内容相同则不另做备份。profile 里那条全局 `ALSA_CONFIG_PATH` 仍是先备份再注释；
旧 `pipewire.conf` 启动覆盖仍改名为 `pipewire.conf.disabled-*`。这些副本不是第二套活动配置。

系统服务在启动时加载并绑定 HDA。用户服务在 PipeWire/WirePlumber 启动后设置 HDA Intel 为默认
sink、取消静音并恢复 mixer，避免 WirePlumber route 恢复把 Speaker 重新关闭。
用户 helper 的现有 `~/.local/bin/` 路径属于兼容接口；未来服务目录规则及迁移要求以
[维护策略](maintenance-policy.md) 为准。

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

## 回退与卸载

`sudo tools/debian/install-hygon-hda-audio.sh --uninstall` 先停用用户单元（有会话时），再停用系统单元，
然后删除 unit、modules-load、helper、指向本 ALSA 配置的 `~/.asoundrc`，以及带本安装器标记的 ALSA 配置。
随后 `daemon-reload`。它不恢复 `.bak-hygon-hda-*`，也不把 profile 注释或 `pipewire.conf.disabled-*` 改回去。
外来内容占着这些路径时卸载直接失败，不先删别的文件。

启用前会跑 `systemd-analyze verify`。有用户会话时，用户单元是 `systemd-analyze --user verify`；没有会话时
只核对单元文件。没有用户会话不是失败：`hygon_hda_user_service=SKIP reason=no_session`，文件仍装上。
会话在时，用户单元的 daemon-reload、enable、restart 失败会回滚这一次安装并退出非 0。
PipeWire 重启和 mixer 仍是尽力而为，不代表用户单元已经成功。

不要把“再跑一次安装”当成卸载。要回到安装前的配置，自己核对时间戳备份再拷回去，不要无条件用最新备份覆盖。
