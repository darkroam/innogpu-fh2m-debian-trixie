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
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-23.deb` | `patch-023` 历史运行包；invisible READ 回写、基础图形验证和 Clash 启动态 A/B 已完成；SHA-256 见 `docs/patches/patch-023-invisible-read-no-writeback.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-24.deb` | patched-23 补丁集合加 `6.12.101+` PCI resize API 兼容；已通过 6.12.101 离线 DKMS 编译；SHA-256 见 `docs/patches/patched-24-kernel-612101.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-25.deb` | patch-025 dma_resv usage 语义修复；已实机验证；当前回退点为 p26；可复现 SHA 见 `debs/README.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-26.deb` | patch-026 未活动 CRTC vblank 守卫；已实机验证；当前回退点为 p27；可复现 SHA 见 `debs/README.md` |
| `debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb` | patch-027 foreign DMA-BUF 生命周期修复；**保留的回退基线**（Phase 4 后设备已运行 4.0.0-i1）；SHA `f3841597…` |
| `build/innogpu-fh2m-trixie_4.0.0-i1.deb`（由 `scripts/build-innogpu-driver.sh` 生成） | **新架构回退包**：迁移源码树 + manifest 黑盒载荷；可复现 epoch 1787342400，SHA `68aea6c0…`；Phase 4 实机验收全 PASS |
| `build/innogpu-fh2m-trixie_4.0.1-i1.deb`（本机失败候选） | patch-024 suspend/resume 实验；固定 epoch 1788278400，SHA `a7fe10ed…`；s2idle 唤醒外屏红屏，已回退，禁止安装 |
| `build/innogpu-fh2m-trixie_4.0.1-i2.deb`（R05 历史候选） | patch-024 + patch-025-suspend-resume-display；固定 epoch 1788364800，历史 SHA `b26c0b27…`；一次 s2idle 可见恢复通过，不作为 R06 严格 A/B 包复用 |
| `build/innogpu-fh2m-trixie_4.0.1-i3.deb`（R06 A/R10 失败候选） | 仅 patch-024；固定 epoch 1788451200；SHA `6cab9e521046b386ec2e34ce84d384302b044a4c215c836566274d5de769dcba`；R10 deep 复现 PowerLock TOCTOU 后已回退 |
| `build/innogpu-fh2m-trixie_4.0.1-i4.deb`（R06 B） | patch-024 + patch-025-suspend-resume-display；固定 epoch 1788451200；SHA `085e06844607a9973a6d5e3c1e3c4ec986a1cdf6903e6a9169b68285e39969a7`；严格对照准备通过，尚未安装 |
| `build/innogpu-fh2m-trixie_4.0.2-i1.deb`（R11 失败候选） | patch-024 + patch-026 DVFS/PVR 生命周期同步；固定 epoch 1788624000；双构建 SHA `e115bdcd…`；deep 时温度 work 提前触发 PowerLock/POWERED_OFF，只供历史复现，禁止安装或交付 |
| `build/innogpu-fh2m-trixie_4.0.2-i2.deb`（R12 历史候选） | i1 + patch-028 温度 work 恢复时序门禁；固定 epoch 1788710400；不含 display 025；仅静态/离线验证，尚未安装 |
| `build/innogpu-fh2m-trixie_4.0.2-i3.deb`（当前正式交付） | i2 + patch-029 DDCCI panel 创建恢复（继承 patch-024/026/028）；固定 epoch 1788796800；SHA `177133ee…`；R14 6/6 deep 通过；不含 display 025，DDCCI 无 backlight device，`hwinfo_g0m.bin` 仍缺失 |
| `debs/innogpu-fh2m_20250421190503-debug_amd64.deb` | Deepin 202504 DKMS/GL/DDX 来源；SHA-256 `b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2` |

新架构提取器默认只读取 `debs/innogpu-fh2m_20250421190503-debug_amd64.deb`，其他位置必须通过
`INNOGPU_DEEPIN_DEB` 显式指定。只有 legacy `build-deepin-coherent.sh` 与
`prepare-deepin-userspace-root.sh` 仍保留仓库根旧路径的兼容查找；后者将 Deepin deb 解包到被 Git
忽略的 `third_party/innogpu-fh2m-deepin-202504/root/`。不得依赖该 legacy 回退构建新架构包。

Deepin 202504 原包是后续版本唯一的来源基线。当前构建器使用 `drivers/` 中已转换的历史源码提交，
`4.0.1-i1`/`i2` 是历史实验候选；R06 的 i3/i4 和失败的 R11 `4.0.2-i1` 保留作复现。当前
`4.0.2-i3` 在 patch-024 后应用 patch-026 生命周期同步、patch-028 温度 work 恢复时序门禁和
patch-029 DDCCI panel 创建恢复，
不含 display 025，并按 manifest 从原包提取同源 DRI、GBM、GLAPI、GLVND、DDX、固件和黑盒对象。
该版本已安装并通过 R14 当前设备 6/6 deep 正式矩阵；历史 patched 包不得作为源码或载荷输入。

历史 p19/p20 deb 的驱动/用户态结论仍可作为证据，但其辅助文件清单不符合当前所有权边界。当前
新构建器只接受按 Debian 版本排序高于 `3.3.3.42-patched-27` 的版本，并必须运行
`scripts/check-release-package.sh`；版本号、包清单和运行证据必须一起更新，不能覆盖旧 deb。

## Debian 包

基础构建与运行依赖由 `scripts/install-prereqs-debian.sh` 安装，主要包括：

- `build-essential`、`dkms`、当前内核 headers、`kmod`、`initramfs-tools`；
- `xserver-xorg-core`、`xinit`、`x11-xserver-utils`、`x11-utils`、`dwm`；
- `mesa-utils`、`libgl1`、`libegl1`、`libgbm1`；
- 音频修复使用的 `alsa-utils`、PipeWire、WirePlumber 和 `wpctl` 所属包；
- Innogpu 显示接入要求目标用户已经从 dotconfig 安装 xdisplay；本项目设备钩子使用 `xrandr`，
  会话接入使用 POSIX shell。

当前新构建器还直接调用 `python3`、`dpkg`/`dpkg-deb`、`sha256sum`、`realpath`、`make` 和
`/usr/sbin/modinfo`。其中 `install-prereqs-debian.sh` 当前没有显式安装 `python3`；最小化 Debian
环境必须在构建前另行确认这些命令可用。该事实是现有前置依赖脚本的覆盖缺口，不得把脚本运行成功
等同于完整工具链已安装。

当前新架构 deb 的实际 control 字段为：`Depends: dkms, build-essential, libdrm2, libepoxy0,
libpixman-1-0, libwayland-server0, libxcb-randr0`，`Recommends: linux-headers-amd64, libegl1,
libgles2, libgl1, libglx0, libgles1, libglvnd0`。包内虽然从 vendor 载荷导入
`/lib/systemd/system/sw-inno-gl.service`，但 control 当前没有显式 `systemd` 依赖，maintainer scripts
也不 enable/start 该单元；这是一项待单独修正和补 release fixture 的实现缺口，而不是服务已激活的证据。

runtime 能力基线另有可选诊断依赖：`pciutils`（`lspci`）、`drm-info`（`drm_info`）、
`vulkan-tools`（`vulkaninfo`）、`clinfo`、`vainfo` 所属发行版包及 `wpctl`。这些工具缺失时对应能力项
必须输出 `SKIP reason=tool_missing:<tool>`，但不影响基础驱动构建、安装、TTY 或 Xorg 启动。

DMA-BUF 回归验证（`tools/run-dmabuf-regression-test.sh`）的 runtime 可选依赖：`gcc` + DRM 头文件
（`/usr/include/drm/drm.h`，Debian Trixie 上属 `linux-libc-dev`）编译四个 C 探针
（`probe-dmabuf-self-import.c`、`probe-pdp-invisible-read.c`、`probe-drm-topology.c`、
`probe-drm-vblank.c`）、`timeout`（coreutils）、`grep`（包名 `grep`）、`awk`（Debian 上包名
`mawk`/`gawk`）。缺失分级：
`gcc` 或头文件缺失 → `dmabuf_tool=fail`（退出码 2）；仅影响真机回归验证的诊断能力，不进入驱动
deb，也不影响基础安装。

VA-API 实际解码验证（`tools/run-vaapi-decode-test.sh`）的 runtime 可选依赖：`ffmpeg`（需编译支持
`--enable-vaapi`、`libx264`、`libx265`）、`vainfo`（Debian Trixie 包名 `vainfo`，源包 `libva-utils`）。
缺失分级：`ffmpeg`/`vainfo` 缺失 → `vaapi_decode_tool=fail`（退出码 2）；缺少 `libx264`/`libx265`
编码器或 h264/hevc 解码器 → 退出码 2；这些仅为真机解码验证的诊断依赖，不进入驱动 deb，也不影响
基础安装。

新增必需依赖时必须同时更新本文和前置依赖安装脚本；可选诊断依赖只需在本文和测试入口登记。
可选命令缺失不能破坏驱动安装、TTY 或 Xorg 基本启动。

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

仓库脚本和文档不得写死真实用户的绝对 home 路径。
