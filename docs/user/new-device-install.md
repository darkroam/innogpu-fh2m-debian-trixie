# 新 Debian 设备安装

## 版本选择

当前版本的职责不同：

| 版本 | 用途 | 新设备策略 |
| --- | --- | --- |
| `patched-21` | 当前设备已完成运行验收的候选 | 跨硬件矩阵、回退演练和 release 审阅完成前，不作为默认入口 |
| `patched-17` | 保守自动安装和 p21 回退包 | 新设备首次安装使用此入口 |
| `patched-8` | 更早历史回滚物 | 仅在 patched-17 无法恢复时使用 |
| `patched-20` | 历史诊断与运行证据 | 禁止部署；旧辅助载荷不符合当前边界 |

patched-18/19 是问题定位和 coherent 构建演进记录，不是安装推荐版本。

## 准备保守安装包

二进制 deb 不随 Git 提供。clone 本仓库后，先从维护者审阅的 release 记录取得以下两个包，并按记录
核对版本和 SHA-256 后放入 `debs/`：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
```

`patched-17` 是本次保守安装包，`patched-8` 只是在下一次启动失败时使用的本地回退包。没有该 release
记录时应停止并向维护者取得包；不得以未知来源 deb 或禁止部署的 patched-20 替代。

Deepin 原包不属于 patched-17 的安装前提。只有构建新的 coherent 候选时才需要另外取得：

```text
debs/innogpu-fh2m_20250421190503-debug_amd64.deb
```

这些文件和本地构建出的 deb 均被 `.gitignore` 忽略。`third_party/` 由脚本从 Deepin 原包重建，
也不进入 Git。

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
```

## 显示引擎前置条件

xdisplay 的唯一源码权威是 dotconfig。本仓库不携带 `xdisplay`、`displayselect`、共享库或引擎测试。
需要自动显示布局时，应先从 dotconfig 安装以下用户组件：

```text
~/.local/bin/xdisplay
~/.local/bin/xdisplay.sh
~/.local/bin/displayselect
~/.local/lib/xdisplay/
```

驱动安装不依赖 xdisplay。缺少上述组件时，patched-17 安装器只警告并跳过显示接入；安装 dotconfig
后可单独执行：

```sh
sudo INNOGPU_X_USER="$USER" INNOGPU_X_HOME="$HOME" \
  scripts/install-xdisplay-user.sh
```

该命令只安装 Innogpu 模式恢复钩子和 X11 会话接入，不会覆盖 dotconfig 的显示引擎。

## 保守入口：patched-17

先确认 patched-8 回退包已经放入 `debs/`，再执行：

```sh
cd "$INNOGPU_ROOT"
sudo scripts/install-patched17-and-check.sh
```

目标桌面用户不是 `SUDO_USER` 时，显式指定用户和主目录：

```sh
sudo INNOGPU_X_USER="$USER" INNOGPU_X_HOME="$HOME" \
  scripts/install-patched17-and-check.sh
```

安装器保持软件 Xorg 用户态，并准备 DKMS、模块自加载和 initramfs。重启后先验证 TTY、驱动节点和
软件 Xorg；需要复现历史硬件 GL 流程时再按 `verification.md` 分步执行。patched-17 的已知边界是
`/dev/fb0 mmap()` 返回 `ENODEV`，因此真实 VT fbterm 会失败。

## 历史候选：patched-20

patched-20 仅保留运行证据，不提供重新部署或回退到该版本的路径；它不是新设备安装选项。当前设备
发生 p21 运行故障时，按 [`recovery.md`](recovery.md) 回退到 patched-17，再在必要时回退到 patched-8。
该 deb 生成于 xdisplay 所有权收敛前，包内仍有旧引擎/实验辅助文件，且 `patch-008` 会高频写日志。
当前 `build-patched20-deepin-diagnostic.sh` 仅作为拒绝版本复用的兼容护栏，不再生成包。

下一候选必须先在 `docs/planning/` 定义新版本号（大于 20）、补丁集合、风险和回退，再从完整
Deepin 202504 原包调用 `build-deepin-coherent.sh`。不得以 patched-8、17、18、19 或 20 的 deb
作为载荷基线，也不得从不同版本挑选 DRI、GBM、GLAPI、DDX 或固件文件拼装。新包还必须通过：

```sh
scripts/check-release-package.sh debs/<new-package>.deb
```

在 p21 完成跨硬件矩阵、回退演练和 release 审阅前，新设备只使用 patched-17 保守入口。

patched-21 已完成当前设备的构建、包边界、部署、重启和运行验收。精确输入、补丁矩阵、清洁载荷边界
和跨硬件发布前门槛见 [`patched-21-release-candidate.md`](../patches/patched-21-release-candidate.md)。
它仍不是本页的新设备默认入口，因为发布矩阵尚未完成。

## 历史失败边界

- patched-18 虽修复 fbdev mmap，但曾以 patched-8 作为其余载荷基线，形成用户态 ABI 混配；同名包
  不应重建或部署。
- patched-18 的另一次试验缺少 `fh2m.sh` shader 固件，导致 PVR services 进入 bad state；不要把
  Xorg 中的 GBM 崩溃误判为单一 `.so` 问题。
- patched-19 首次恢复了完整 Deepin 202504 coherent 载荷，只保留为 patched-20 的构建演进记录。

详细证据见 [`docs/incidents/`](../incidents/README.md) 和
[`docs/patches/`](../patches/README.md)。

## 内置喇叭

```sh
sudo scripts/install-hygon-hda-audio.sh
```

需要测试音时使用 `sudo scripts/install-hygon-hda-audio.sh --test-sound`。音频恢复不要求重装
Innogpu DKMS。

## 可选 Picom

显卡驱动、Xorg 和硬件 GL 验证完成后，再按 [`picom-install.md`](picom-install.md) 安装 patched
Picom。不要在首次驱动启动验证前同时启用合成器。
