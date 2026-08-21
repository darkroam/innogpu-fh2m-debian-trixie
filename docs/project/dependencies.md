# 依赖与外部文件

## Release 文件

以下 `.deb` 不进入 git，下载或构建后放在 `debs/`：

| 文件 | 用途 |
| --- | --- |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb` | 早期历史回滚物，不用于后续构建或实现参考 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb` | 当前可用回退包，不作为后续构建基线 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-18.deb` | framebuffer mmap 历史候选，不作为后续基线 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-19.deb` | 完整 Deepin 202504 载荷历史候选；含收敛前辅助载荷，不发布、不重建 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-20.deb` | 历史诊断候选；含收敛前辅助载荷，只保留证据，不用于新设备 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb` | 当前设备已完成运行验收的候选；跨硬件发布审阅前不作为新设备默认入口 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-22.deb` | `patch-009` 直接回退包；connector/桌面烟测已通过，完整电源与合盖矩阵待完成 |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-23.deb` | `patch-023` 当前运行包；invisible READ 回写、基础图形验证和 Clash 启动态 A/B 已完成；SHA-256 见 `docs/patches/patch-023-invisible-read-no-writeback.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-24.deb` | patched-23 补丁集合加 `6.12.101+` PCI resize API 兼容；已通过 6.12.101 离线 DKMS 编译；SHA-256 见 `docs/patches/patched-24-kernel-612101.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-25.deb` | patch-025 dma_resv usage 语义修复；已实机验证；当前回退点为 p26；可复现 SHA 见 `debs/README.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-26.deb` | patch-026 未活动 CRTC vblank 守卫；已实机验证；当前回退点为 p27；可复现 SHA 见 `debs/README.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb` | patch-027 foreign DMA-BUF 生命周期修复；**保留的回退基线**（Phase 4 后设备已运行 4.0.0-i1）；SHA `f3841597…` |
| `build/innogpu-fh2m-trixie_4.0.0-i1.deb`（由 `scripts/build-innogpu-driver.sh` 生成） | **新架构当前运行包**：迁移源码树 + manifest 黑盒载荷；可复现 epoch 1787342400，SHA `68aea6c0…`；Phase 4 实机验收全 PASS |
| `debs/innogpu-fh2m_20250421190503-debug_amd64.deb` | Deepin 202504 DKMS/GL/DDX 来源 |

构建和准备脚本优先查找 `debs/`，并保留仓库根目录旧路径作为兼容回退。
`scripts/prepare-deepin-userspace-root.sh` 将 Deepin deb 解包到被 git 忽略的
`third_party/innogpu-fh2m-deepin-202504/root/`。

Deepin 202504 原包是后续版本唯一的技术基线。构建必须保留其中同源的 DRI、GBM、GLAPI、GLVND
和 DDX 载荷，只允许在其 DKMS 源码上叠加仓库补丁；历史 patched 包不得再作为 `BASE_DEB`。

历史 p19/p20 deb 的驱动/用户态结论仍可作为证据，但其辅助文件清单不符合当前所有权边界。当前源码
只能生成版本号大于 20 的新候选，并必须运行 `scripts/check-release-package.sh`；版本号、包清单和
运行证据必须一起更新，不能用相同版本号覆盖旧 deb。

## Debian 包

基础构建与运行依赖由 `scripts/install-prereqs-debian.sh` 安装，主要包括：

- `build-essential`、`dkms`、当前内核 headers、`kmod`、`initramfs-tools`；
- `xserver-xorg-core`、`xinit`、`x11-xserver-utils`、`x11-utils`、`dwm`；
- `mesa-utils`、`libgl1`、`libegl1`、`libgbm1`；
- 音频修复使用的 `alsa-utils`、PipeWire、WirePlumber 和 `wpctl` 所属包；
- Innogpu 显示接入要求目标用户已经从 dotconfig 安装 xdisplay；本项目设备钩子使用 `xrandr`，
  会话接入使用 POSIX shell。

新增脚本依赖时必须先更新本文和前置依赖安装脚本。可选命令缺失不能破坏驱动安装、TTY 或 Xorg
基本启动。

xdisplay、`displayselect` 及其 `flock`、`timeout`、`dmenu`、`arandr`、`bc` 等依赖由 dotconfig
声明和安装，本仓库不复制其依赖清单。Innogpu 接入变化时只需验证兼容环境变量、恢复钩子和会话入口。

## Picom 外部源码

Picom 源码不复制进本仓库。当前基线为上游 `yshui/picom` 的 `next` 提交：

```text
6d676824c457a933c52e3e92c5a1856466f90545
```

构建依赖和安装命令由 `scripts/install-picom-prereqs-debian.sh` 与
`scripts/build-patched-picom.sh` 管理。`xcompmgr` 是 Picom 二进制不存在时的轻量回退，不用于验证
GLX 补丁。

## fbterm 外部源码

fbterm 源码不复制进本仓库。兼容补丁固定面向 Debian `fbterm 1.7-5` 源码包，默认源码路径为
`~/src/fbterm-1.7`。使用 Debian source repository 取得源码后，通过以下入口构建和安装：

```sh
scripts/build-patched-fbterm.sh
scripts/build-patched-fbterm.sh --install
```

构建需要 C/C++ 工具链、`patch`、pkg-config、FreeType 和 Fontconfig 开发文件；已验证变体关闭当前
设备不使用的 GPM 与 legacy VESA 支持。安装目标默认为 `~/.local/bin/fbterm`，不会覆盖 Debian 的
`/usr/bin/fbterm`。

## 路径约定

```sh
INNOGPU_ROOT=${INNOGPU_ROOT:-$HOME/src/innogpu-fh2m-debian-trixie}
INNOGPU_DEEPIN_ROOT=${INNOGPU_DEEPIN_ROOT:-$INNOGPU_ROOT/third_party/innogpu-fh2m-deepin-202504/root}
INNOGPU_X_USER=${INNOGPU_X_USER:-$USER}
INNOGPU_X_HOME=${INNOGPU_X_HOME:-$HOME}
```

仓库脚本和文档不得写死 `/home/ok`。
