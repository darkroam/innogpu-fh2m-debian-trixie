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

若对应文件原已存在，安装器还会按时间戳保留 `asoundrc.bak-*`、`.profile.bak-hygon-hda-*` 或
`.zprofile.bak-hygon-hda-*`；检测到旧 `~/.config/pipewire/pipewire.conf` 启动覆盖时会改名为
`pipewire.conf.disabled-*`。这些是条件性回退副本，不是第二套活动配置。备份范围并不覆盖全部写入：
系统 unit、modules-load 文件、用户 helper/unit 与 `~/.asoundrc` 当前会被直接创建或替换，安装器不会
为它们保留原文件；执行前必须由操作者另行确认这些路径没有需要保留的本机配置。

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

## 回退与卸载边界

当前仓库只有安装/重应用入口，没有与 `install-hygon-hda-audio.sh` 对称的自动卸载器。安装器会创建
系统 unit、用户 unit、helper、modules-load 配置和 ALSA 配置，还可能修改 profile 并重命名旧
PipeWire 配置；因此不能把“再次运行安装器”描述为卸载或完整回退。

需要撤销时必须先核对目标用户和时间戳备份，再分别停用系统/用户 unit，删除安装器创建的活动文件、
执行对应的 `daemon-reload`，并按需恢复 `asoundrc.bak-*`、`.profile.bak-hygon-hda-*`、
`.zprofile.bak-hygon-hda-*` 和 `pipewire.conf.disabled-*`。在独立卸载脚本及 fixture 测试落地前，
该流程属于人工维护项；不要无条件选择“最新备份”覆盖现有配置，也不要假定没有备份的
`~/.asoundrc`、unit、helper 或 modules-load 原内容能够由安装器恢复。当前安装器也没有在写入后运行
`systemd-analyze verify`；服务成功 restart 只能证明当前 systemd 接受并启动了系统 unit，用户 unit
的 daemon-reload/enable/restart 失败则会被降级忽略，仍需在目标用户会话中单独验证。
