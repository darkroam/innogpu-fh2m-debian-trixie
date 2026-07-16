# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上 Innosilicon Fantasy II-M / 风华2号M 的驱动打包、兼容修复、
显示管理、硬件 GL、内置音频和验证项目。

## 当前结论

| 项目 | 当前状态 |
| --- | --- |
| 驱动包 | `3.3.3.42-patched-17`，基于 Deepin 202504 DKMS 源码 |
| 回退点 | `patched-8`，保留且不继续修改 |
| Debian 6.12 / DKMS | 通过 |
| tty1、Xorg、dwm | 通过 |
| DRM/fbdev | `card0`、`renderD128`、`fb0` 可用 |
| 硬件 GL | renderer 为 `Fantasy II-M`，历史验证通过 |
| 内置喇叭 | `1d94:14c9 -> snd_hda_intel -> SN6180`，重启验证通过 |
| 显示 watcher | 已吸纳；fixture、安装器、测试包构建和当前 X11 只读状态通过 |

“历史通过”不能替代当前运行检查。安装后应执行 `docs/user/verification.md` 中的命令。

## 快速开始

从 release 下载以下文件到仓库根目录；它们被 git 忽略：

```text
innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
innogpu-fh2m_20250421190503-debug_amd64.deb
```

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
sudo scripts/install-patched17-and-check.sh
```

重启或启用硬件 GL 前，先阅读：

- [新设备安装](docs/user/new-device-install.md)
- [状态验证](docs/user/verification.md)
- [故障恢复](docs/user/recovery.md)

内置喇叭配置：

```sh
sudo scripts/install-hygon-hda-audio.sh
```

## 文档

### 项目现状

- [项目架构](docs/project/architecture.md)
- [显示管理](docs/project/display-management.md)
- [音频管理](docs/project/audio-management.md)
- [依赖与外部文件](docs/project/dependencies.md)
- [维护策略](docs/project/maintenance-policy.md)

### 计划与历史

- [当前 TODO](docs/planning/todo.md)
- [显示代码吸纳计划](docs/planning/display-integration.md)
- [实施历史](docs/planning/history.md)
- [挂起项](docs/planning/suspended.md)

### 用户说明

- [新设备安装](docs/user/new-device-install.md)
- [状态验证](docs/user/verification.md)
- [故障恢复](docs/user/recovery.md)
- [显示切换](docs/user/display-guide.md)

历史材料见 [docs/archive](docs/archive/)，精简验证证据见 [baselines](baselines/README.md)。

## 仓库结构

```text
.
|-- README.md
|-- patches/
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
