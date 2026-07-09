# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上的 Innosilicon Fantasy II-M / 风华2号M 驱动打包、兼容修复和验证记录。

## 当前结论

当前可用版本是 `innogpu-fh2m-trixie 3.3.3.42-patched-17`。

| 项目 | 状态 |
| --- | --- |
| DKMS 内核模块 | 通过，基于 Deepin 202504 DKMS 源码 |
| Debian 6.12 兼容 | 通过，包含 kernel 6.12 修复 |
| 本机启动显示 | 通过，tty1、Xorg、dwm 可用 |
| DRM/fbdev 节点 | 通过，`card0`、`renderD128`、`fb0` 可用 |
| Deepin 用户态硬件 GL | 通过 |
| 当前 OpenGL renderer | `Fantasy II-M` |
| 内置喇叭 | 通过，`06:00.6 [1d94:14c9]` 手动绑定 `snd_hda_intel` 后为 `SN6180 Analog` |
| 回退点 | `patched-8` 和 `patched-17` 均保留 |

最终验证基线见 [baselines/final-summary.md](baselines/final-summary.md)。

## 路径约定

脚本不再写死本机用户目录。默认约定如下，可按实际环境覆盖：

```bash
export INNOGPU_ROOT="${INNOGPU_ROOT:-$HOME/src/innogpu-fh2m-debian-trixie}"
export INNOGPU_DEEPIN_ROOT="${INNOGPU_DEEPIN_ROOT:-$INNOGPU_ROOT/third_party/innogpu-fh2m-deepin-202504/root}"
export INNOGPU_X_USER="${INNOGPU_X_USER:-$USER}"
export INNOGPU_X_HOME="${INNOGPU_X_HOME:-$HOME}"
```

## 保留的关键包

| 文件 | 用途 |
| --- | --- |
| `innogpu-fh2m-trixie_3.3.3.42-patched-8.deb` | 原始稳定回退点，不修改 |
| `innogpu-fh2m-trixie_3.3.3.42-patched-17.deb` | 当前成功版本，Deepin 202504 基线 + 本设备显示修复 + 硬件 GL |
| `innogpu-fh2m_20250421190503-debug_amd64.deb` | Deepin 202504 用户态 GL/DDX 来源 |

这些 `.deb` 不纳入 git；从 release 独立下载后放到仓库根目录即可。`.gitignore` 会忽略所有 `.deb`。

Deepin deb 放置和解包位置：

| 内容 | 位置 | 是否进 git |
| --- | --- | --- |
| Deepin 原始包 | `./innogpu-fh2m_20250421190503-debug_amd64.deb` | 否 |
| Deepin 解包目录 | `./third_party/innogpu-fh2m-deepin-202504/root/` | 否 |
| patched-17 构建用 DKMS 源码 | `./third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2/` | 否，脚本自动从 deb 解包 |

`scripts/prepare-deepin-userspace-root.sh` 会从仓库根目录的 Deepin deb 解包出 `third_party/.../root`。`scripts/build-patched17-deepin-local-display.sh` 构建 patched-17 时会使用其中的 Deepin DKMS 源码，再叠加本仓库 `patches/` 中的本机适配补丁。

## 新设备安装

新 Debian Trixie 设备 clone 本仓库后，先从 release 下载显卡驱动安装所需的两个 patched 包和 Deepin 202504 用户态 deb，并放到仓库根目录。

还需要通过 Debian 安装系统依赖：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
```

完整流程见 [docs/new-device-install.md](docs/new-device-install.md)。

安装状态验证：

```bash
cd "$INNOGPU_ROOT"
scripts/verify-install-status.sh
```

如果要验证当前确实是指定版本：

```bash
scripts/verify-install-status.sh 3.3.3.42-patched-17
scripts/verify-install-status.sh 3.3.3.42-patched-8
```

## 构建 patched-17

```bash
cd "$INNOGPU_ROOT"
scripts/build-patched17-deepin-local-display.sh
```

`patched-17` 的构建逻辑：

- 以 `patched-8` 包装和安全回退能力为基础。
- 替换为 Deepin 202504 DKMS 源码。
- 应用 Debian 6.12 兼容补丁。
- 应用本设备需要的 local connector ACPI map 和 DP/fbcon fallback。
- 不再沿用旧版本 DKMS 源码继续堆补丁。

## 安装和验证

安装当前成功包：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/install-patched17-and-check.sh
```

验证当前状态：

```bash
cd "$INNOGPU_ROOT"
scripts/check-innogpu-progress.sh
scripts/check-desktop-hwgl.sh
sudo scripts/check-post-reboot-hwgl.sh
```

内置喇叭修复：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/install-hygon-hda-audio.sh
```

该脚本会：

- 加载 `snd_hda_intel`。
- 将 `0000:06:00.6 [1d94:14c9]` 绑定到 `snd_hda_intel`。
- 安装 `hygon-hda-audio.service`，开机后自动恢复绑定。
- 安装用户级 `hygon-hda-audio-user.service`，在 PipeWire/WirePlumber 启动后恢复默认输出和喇叭 mixer。
- 保留 `~/.config/alsa/asoundrc` 作为用户 ALSA 配置文件，并通过 `~/.asoundrc` 标准方式加载。
- 禁用全局 `ALSA_CONFIG_PATH`，避免 PipeWire/WirePlumber 无法枚举硬件输出。
- 将 PipeWire 默认输出切到 `HDA Intel 模拟立体声`。
- 打开 `Master`、`Speaker`，并关闭 HDA `Auto-Mute Mode`。

可选播放测试音：

```bash
sudo scripts/install-hygon-hda-audio.sh --test-sound
```

期望结果：

```text
PASS_DESKTOP_HWGL
PASS_POST_REBOOT_HWGL
PASS_CURRENT_XORG_HWGL_RUNTIME
PASS_VENDOR_DDX_RUNTIME_ACCELERATION
```

## 回退流程

如果 patched-17 安装或重启后显示异常，优先回退 patched-8：

```bash
cd "$INNOGPU_ROOT"
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
sudo scripts/disable-incompatible-userspace.sh
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```

如果只需要恢复 tty1 登录提示，不安装包、不重启：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/restore-tty1-login.sh
```

如果只需要恢复软件渲染 Xorg/dwm：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/prepare-soft-xorg-dwm.sh
```

## 主要脚本

| 脚本 | 说明 |
| --- | --- |
| `scripts/build-patched17-deepin-local-display.sh` | 构建当前成功包 |
| `scripts/build-patched10-deepin.sh` | 通用 Deepin DKMS 基线打包入口 |
| `scripts/install-prereqs-debian.sh` | 新 Debian 系统安装前置依赖 |
| `scripts/install.sh` | 当前主安装入口，默认安装 patched-17 |
| `scripts/install-patched17-and-check.sh` | 安装 patched-17 并做基础检查 |
| `scripts/install-patched8-and-check.sh` | 安装 patched-8 回退点 |
| `scripts/install-hygon-hda-audio.sh` | 持久化内置 HDA 喇叭修复 |
| `scripts/uninstall-patched17.sh` | 卸载当前 patched-17 包 |
| `scripts/uninstall-patched8.sh` | 卸载当前 patched-8 包 |
| `scripts/prepare-deepin-userspace-root.sh` | 从仓库内 Deepin deb 解包用户态库 |
| `scripts/verify-install-status.sh` | 只读验证安装状态、DKMS、节点和 Xorg/GL 状态 |
| `scripts/check-innogpu-progress.sh` | 汇总当前系统状态 |
| `scripts/check-desktop-hwgl.sh` | 验证当前桌面硬件 GL |
| `scripts/check-post-reboot-hwgl.sh` | 验证重启后持久状态 |
| `scripts/disable-incompatible-userspace.sh` | 禁用不兼容用户态并回到安全配置 |
| `scripts/prepare-soft-xorg-dwm.sh` | 准备软件渲染 Xorg/dwm |
| `scripts/restore-tty1-login.sh` | 恢复 tty1 用户名密码登录提示 |
| `scripts/repair-dri-nodes.sh` | 修复缺失的 DRM/fbdev 节点 |

## 代码库结构

```text
.
├── README.md
├── LICENSE
├── .gitignore
├── patches/
├── scripts/
├── tools/
├── docs/
├── baselines/
├── third_party/
└── *.deb
```

| 路径 | 说明 |
| --- | --- |
| `patches/` | Debian 6.12 兼容、本机显示/TTY/fbcon 修复补丁。 |
| `scripts/` | 构建、安装、回退、硬件 GL 启用、状态验证和显示恢复脚本。 |
| `tools/` | EGL/GBM/X11 最小化探针源码，用于排查用户态 GL。 |
| `docs/` | 新设备安装、清理过程、维护说明。 |
| `baselines/` | 最终验证结果和精简基线，不再保存大量原始 Xorg 日志。 |
| `third_party/` | Deepin 202504 deb 的解包输出目录；该目录不进 git，可由脚本重建。 |
| `*.deb` | release 下载到仓库根目录的外部包；被 `.gitignore` 忽略，不进 git。 |

关键入口：

| 入口 | 用途 |
| --- | --- |
| `scripts/install.sh` | 默认安装 patched-17，可加 `--prereqs` 安装 Debian 依赖。 |
| `scripts/verify-install-status.sh` | 安装后只读验证包、DKMS、驱动、节点、Xorg/GL 状态。 |
| `scripts/prepare-deepin-userspace-root.sh` | 从根目录 Deepin deb 解包用户态库到 `third_party/`。 |
| `scripts/install-patched8-and-check.sh` | 安装 patched-8 回退点。 |
| `scripts/install-patched17-and-check.sh` | 安装当前 patched-17 成功点。 |

## 文档

| 文件 | 说明 |
| --- | --- |
| [docs/new-device-install.md](docs/new-device-install.md) | 新 Debian 设备从 clone 到安装/卸载 |
| [docs/cleanup-20260708.md](docs/cleanup-20260708.md) | 本次仓库整理过程和取舍 |
| [baselines/final-summary.md](baselines/final-summary.md) | 最终成功基线摘要 |
