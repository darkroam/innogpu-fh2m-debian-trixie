# 依赖与外部文件

## Release 文件

以下 `.deb` 不进入 git，下载或构建后放在 `debs/`：

| 文件 | 用途 |
| --- | --- |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb` | 早期历史回滚物，不用于后续构建或实现参考 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb` | 当前可用回退包，不作为后续构建基线 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-18.deb` | framebuffer mmap 历史候选，不作为后续基线 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-19.deb` | 完整 Deepin 202504 载荷历史候选，已通过离线一致性检查 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-20.deb` | 当前安装并通过 PVR、Xorg/GLX、fbterm 验收的诊断候选 |
| `debs/innogpu-fh2m_20250421190503-debug_amd64.deb` | Deepin 202504 DKMS/GL/DDX 来源 |

构建和准备脚本优先查找 `debs/`，并保留仓库根目录旧路径作为兼容回退。
`scripts/prepare-deepin-userspace-root.sh` 将 Deepin deb 解包到被 git 忽略的
`third_party/innogpu-fh2m-deepin-202504/root/`。

Deepin 202504 原包是后续版本唯一的技术基线。构建必须保留其中同源的 DRI、GBM、GLAPI、GLVND
和 DDX 载荷，只允许在其 DKMS 源码上叠加仓库补丁；历史 patched 包不得再作为 `BASE_DEB`。

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

## Picom 外部源码

Picom 源码不复制进本仓库。当前基线为上游 `yshui/picom` 的 `next` 提交：

```text
6d676824c457a933c52e3e92c5a1856466f90545
```

构建依赖和安装命令由 `scripts/install-picom-prereqs-debian.sh` 与
`scripts/build-patched-picom.sh` 管理。`xcompmgr` 是 Picom 二进制不存在时的轻量回退，不用于验证
GLX 补丁。

## 路径约定

```sh
INNOGPU_ROOT=${INNOGPU_ROOT:-$HOME/src/innogpu-fh2m-debian-trixie}
INNOGPU_DEEPIN_ROOT=${INNOGPU_DEEPIN_ROOT:-$INNOGPU_ROOT/third_party/innogpu-fh2m-deepin-202504/root}
INNOGPU_X_USER=${INNOGPU_X_USER:-$USER}
INNOGPU_X_HOME=${INNOGPU_X_HOME:-$HOME}
```

仓库脚本和文档不得写死 `/home/ok`。
