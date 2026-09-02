# 新 Debian 设备安装

## 版本选择

当前版本的职责不同：

| 版本 | 用途 | 新设备策略 |
| --- | --- | --- |
| `4.0.1-i4` | R06 B：patch-024 + patch-025-suspend-resume-display | 包级单变量候选；因 A 的 cursor 分支未入组而未安装，不是新设备默认版本 |
| `4.0.1-i3` | R06 A：patch-024 | 当前实验运行版本；首轮 s2idle 正常但未复现且 cursor 分支未入组，不是新设备默认版本 |
| `4.0.1-i2` | R05 历史候选 | 一次 s2idle 可见恢复通过，但不作为 R06 严格 A/B 包复用 |
| `4.0.1-i1` | patch-024 suspend/resume 失败候选 | s2idle 唤醒红屏，已回退；显式版本/epoch 仅供离线复现，禁止安装 |
| `4.0.0-i1` | **新架构回退包**（迁移源码树 + manifest 黑盒载荷，Phase 4 实机验收通过；deep resume 已知故障） | 只使用已留存且核对过 SHA 的历史构建，不得用当前源码复用版本号 |
| `patched-27` | 保留的回退基线（SHA `f3841597…`） | `4.0.0-i1` 故障时的首选回退（`--allow-downgrades`） |
| `patched-17` | 保守历史回退包 | 深层回退点（保留，不再作为默认入口） |
| `patched-8` | 更早历史回滚物 | 仅在 patched-17 无法恢复时使用 |
| `patched-24`~`patched-21` | 历史运行候选与验收基线 | 仅作回退链与证据，不作为新设备入口 |
| `patched-20` | 历史诊断与运行证据 | 禁止部署；旧辅助载荷不符合当前边界 |

patched-18/19 是问题定位和 coherent 构建演进记录，不是安装推荐版本。

## 构建 suspend/resume 候选（禁止未审查安装）

新架构包不随 Git 提供。clone 本仓库后，从有权提供该内容的来源取得 Deepin 原包并放入 `debs/`；
本项目当前不提供该第三方原包或载荷的公开下载。构建器会按 `binary-manifest.json` 校验完整
SHA-256。R06 i3/i4 共用固定审核 epoch `1788451200`（2026-09-04 00:00 +0800）：

```text
debs/innogpu-fh2m_20250421190503-debug_amd64.deb
SHA-256: b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2
```

```sh
cd "$INNOGPU_ROOT"
sha256sum debs/innogpu-fh2m_20250421190503-debug_amd64.deb
bash scripts/extract-vendor-binaries.sh                        # 按 manifest 重建 vendor/ 黑盒载荷
SOURCE_DATE_EPOCH=1788451200 bash scripts/build-innogpu-driver.sh
# 默认输出 build/innogpu-fh2m-trixie_4.0.1-i4.deb；未经 R06 复审不得安装
# A 对照：VERSION=4.0.1-i3 SOURCE_DATE_EPOCH=1788451200 bash scripts/build-innogpu-driver.sh
```

> 新 clone 上 `vendor/` 为空（不入库）：必须先 `extract-vendor-binaries.sh` 重建黑盒载荷，
> 构建器的 `--check-only` 门禁才会通过。

同时保留深层回退包（按 release 记录核对 SHA-256 后放入 `debs/`）：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
```

`patched-27` 是 `4.0.0-i1` 的首选回退包，`patched-17`/`patched-8` 是深层回退包。没有 release
记录时应停止并向维护者取得包；不得以未知来源 deb 或禁止部署的 patched-20 替代。

这些文件和本地构建出的 deb 均被 `.gitignore` 忽略。`third_party/` 由脚本从 Deepin 原包重建，
也不进入 Git。

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
```

该入口安装当前列出的基础构建/运行包，但尚未显式安装新构建器直接调用的 `python3`。最小化 Debian
环境继续构建前，按 [`dependencies.md`](../project/dependencies.md) 核对 `python3`、`dpkg-deb`、
`sha256sum`、`realpath`、`make` 与 `modinfo`；不能只凭前置脚本退出 0 判断完整工具链已经就绪。

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

## 已验证现存包：4.0.0-i1

当前 HEAD 不再构建 `4.0.0-i1`，因为 patch-024/025 已改变源码行为。只有已留存且哈希与 Phase 4 记录
一致的 4.0.0-i1 包可用于恢复现有基线；它存在已知 deep resume 故障。安装前确认 `patched-27`
回退包在 `debs/`，并保留真实 TTY/物理电源键恢复通道；按
[`phase4-device-validation.md`](../planning/phase4-device-validation.md) 先做 B1-B12 基线采集。

```sh
cd "$INNOGPU_ROOT"
sudo apt install ./build/innogpu-fh2m-trixie_4.0.0-i1.deb
# 安装后暂停，重启后再做 A1-A12 验收；任一失败按 recovery.md 回退 patched-27
```

## 深层回退入口：patched-17（legacy，保留）

仅在 `4.0.0-i1 → patched-27` 仍无法恢复时使用。先确认 patched-8 回退包已经放入 `debs/`，再执行：

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

patched-20 仅保留运行证据，不提供重新部署或回退到该版本的路径；它不是新设备安装选项。当前
`4.0.0-i1` 故障时先按 [`recovery.md`](recovery.md) 回退 patched-27，只有深层恢复才使用
patched-17/patched-8。
该 deb 生成于 xdisplay 所有权收敛前，包内仍有旧引擎/实验辅助文件，且 `patch-008` 会高频写日志。
当前 `build-patched20-deepin-diagnostic.sh` 仅作为拒绝版本复用的兼容护栏，不再生成包。

`4.0.1-i1` 已判定为失败候选；`4.0.1-i2` 是 R05 历史候选；当前运行 R06 A=i3，
但首轮未复现红屏且 cursor 分支未入组，i3/i4 因果验证已按停止条件中止。后续任何行为变化
仍须升新迭代号。
不得以任何 patched deb
作为源码或载荷基线，也不得从不同版本挑选 DRI、GBM、GLAPI、DDX 或固件拼装。新包还必须通过：

```sh
scripts/check-release-package.sh build/<new-package>.deb
```

`4.0.1-i1` 已完成离线构建、安装与 s2idle 验收，但因红屏失败；`4.0.1-i2` 已完成一次
s2idle 可见恢复；i3/i4 只完成离线构建和包级单变量准备。当前 HEAD **没有自动可安装的新设备默认版本**。
4.0.0-i1 仅是现存已验证基线且有 deep 已知故障，patched-17 仅作为深层回退保留。

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
