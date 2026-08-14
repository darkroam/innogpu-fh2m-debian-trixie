# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上 Innosilicon Fantasy II-M / 风华2号M 的驱动打包、兼容修复、
显示管理、硬件 GL、内置音频和验证项目。

## 当前结论

| 项目 | 当前状态 |
| --- | --- |
| 驱动包 | 当前运行并已验收 `3.3.3.42-patched-20`，基于完整 Deepin 202504 原包 |
| 回退点 | `patched-17`；`patched-8` 仅保留为更早的历史回滚物 |
| Debian 6.12 / DKMS | 通过 |
| tty1、Xorg、dwm | 通过 |
| DRM/fbdev | `card0`、`renderD128`、`fb0` 节点、ioctl 和 `mmap()` 可用；真实 VT `fbterm` 通过 |
| 硬件 GL | renderer 为 `Fantasy II-M`，Xorg/GLX direct rendering 和加速通过 |
| Picom GLX | patched v13 正在运行，圆角、模糊、动画保留，全局透明关闭 |
| 内置喇叭 | `1d94:14c9 -> snd_hda_intel -> SN6180`，重启验证通过 |
| 显示 watcher | 已吸纳；fixture、安装器、测试包构建和当前 X11 只读状态通过 |

`patched-18` 是针对 patched-17 framebuffer `mmap()` 返回 `ENODEV` 的历史候选；其旧构建流程曾
混用 patched-8 用户态载荷，不能作为后续基线。该故障确认后续版本必须以 Deepin 202504 原包的
完整载荷为唯一技术基线，不能再以 patched-8 或 patched-17/18 的包内容继续派生。

`patched-19` 是按上述规则完成的第一个完整载荷候选，作为 patched-20 的构建基础和历史记录保留。
当前已安装并验收的 patched-20 在此基础上保留 `fb_io_mmap`，并加入临时 PVR 初始化诊断；正式长期
运行包应在确认后移除或限速该诊断补丁。

“历史通过”不能替代当前运行检查。安装后应执行 `docs/user/verification.md` 中的命令。

## 快速开始

从 release 下载以下文件到仓库根目录；它们被 git 忽略：

```text
innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
innogpu-fh2m-trixie_3.3.3.42-patched-20.deb
innogpu-fh2m_20250421190503-debug_amd64.deb
```

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
# The existing helper installs the patched-17 fallback package.
sudo scripts/install-patched17-and-check.sh
```

新设备部署当前已验收的 patched-20 时，应使用 release 中的
`innogpu-fh2m-trixie_3.3.3.42-patched-20.deb` 和对应安装流程；`install-patched17-and-check.sh`
仅用于保留的 patched-17 回退路径。

重启或启用硬件 GL 前，先阅读：

- [新设备安装](docs/user/new-device-install.md)
- [状态验证](docs/user/verification.md)
- [故障恢复](docs/user/recovery.md)

内置喇叭配置：

```sh
sudo scripts/install-hygon-hda-audio.sh
```

patched Picom GLX 的独立安装见 [Picom 安装与恢复](docs/user/picom-install.md)。它不随显卡驱动 deb
自动安装，失败时也不需要回退 DKMS。

## 文档

### 项目现状

- [项目架构](docs/project/architecture.md)
- [显示管理](docs/project/display-management.md)
- [Picom 合成器](docs/project/compositor-management.md)
- [音频管理](docs/project/audio-management.md)
- [依赖与外部文件](docs/project/dependencies.md)
- [维护策略](docs/project/maintenance-policy.md)

### 计划与历史

- [当前 TODO](docs/planning/todo.md)
- [显示代码吸纳计划](docs/planning/display-integration.md)
- [Picom 吸纳计划](docs/planning/picom-integration.md)
- [实施历史](docs/planning/history.md)
- [挂起项](docs/planning/suspended.md)

### 用户说明

- [新设备安装](docs/user/new-device-install.md)
- [状态验证](docs/user/verification.md)
- [故障恢复](docs/user/recovery.md)
- [显示切换](docs/user/display-guide.md)
- [Picom 安装与恢复](docs/user/picom-install.md)

历史材料见 [docs/archive](docs/archive/)，精简验证证据见 [baselines](baselines/README.md)。

## 仓库结构

```text
.
|-- README.md
|-- patches/
|-- config/
|-- scripts/
|-- tools/
|-- tests/
|-- docs/
|   |-- project/
|   |-- planning/
|   |-- user/
|   `-- archive/
|-- baselines/
|-- third_party/    # 生成目录，不进入 git
`-- *.deb           # release 文件，不进入 git
```

## 维护规则

代码修改前先更新文档中的目标、风险、验证和回退；代码完成后再次复核文档，只把实际通过的行为
标记为当前生效。完整规则见 [维护策略](docs/project/maintenance-policy.md)。

不要提交外部 deb、原始日志、缓存、EDID、凭据、本机绝对 home 路径或 `third_party/` 解包输出。
