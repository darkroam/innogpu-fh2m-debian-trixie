# 依赖与外部文件

## Release 文件

以下 `.deb` 不进入 git，下载后放在仓库根目录：

| 文件 | 用途 |
| --- | --- |
| `innogpu-fh2m-trixie_3.3.3.42-patched-8.deb` | 稳定显示回退点 |
| `innogpu-fh2m-trixie_3.3.3.42-patched-17.deb` | 当前成功驱动包 |
| `innogpu-fh2m_20250421190503-debug_amd64.deb` | Deepin 202504 DKMS/GL/DDX 来源 |

`scripts/prepare-deepin-userspace-root.sh` 将 Deepin deb 解包到被 git 忽略的
`third_party/innogpu-fh2m-deepin-202504/root/`。

## Debian 包

基础构建与运行依赖由 `scripts/install-prereqs-debian.sh` 安装，主要包括：

- `build-essential`、`dkms`、当前内核 headers、`kmod`、`initramfs-tools`；
- `xserver-xorg-core`、`xinit`、`x11-xserver-utils`、`x11-utils`、`dwm`；
- `mesa-utils`、`libgl1`、`libegl1`、`libgbm1`；
- 音频修复使用的 `alsa-utils`、PipeWire、WirePlumber 和 `wpctl` 所属包；
- 显示管理使用的 `xrandr`、`flock`、`dmenu`、`arandr`、`bc`、POSIX `awk`、`stat` 和
  `timeout`；其中 `flock`/`setsid` 由 `util-linux` 提供。

新增脚本依赖时必须先更新本文和前置依赖安装脚本。可选命令缺失不能破坏驱动安装、TTY 或 Xorg
基本启动。

`displayselect` 可选使用 `setbg`、`remaps`、`dunst` 和 `notify-send` 完成桌面后处理，这些命令
不是项目安装依赖；缺失或失败时布局操作仍应成功。

## 路径约定

```sh
INNOGPU_ROOT=${INNOGPU_ROOT:-$HOME/src/innogpu-fh2m-debian-trixie}
INNOGPU_DEEPIN_ROOT=${INNOGPU_DEEPIN_ROOT:-$INNOGPU_ROOT/third_party/innogpu-fh2m-deepin-202504/root}
INNOGPU_X_USER=${INNOGPU_X_USER:-$USER}
INNOGPU_X_HOME=${INNOGPU_X_HOME:-$HOME}
```

仓库脚本和文档不得写死 `/home/ok`。
