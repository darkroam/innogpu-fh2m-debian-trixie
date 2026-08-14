# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上 Innosilicon Fantasy II-M / 风华 2 号 M 的驱动打包、兼容修复、
设备接入、硬件 GL、内置音频和验收项目。本文件是项目文档的唯一入口；当前结论以
[`docs/project/status.md`](docs/project/status.md) 为权威来源。

## 当前状态与结论

最后更新：2026-08-14。

| 项目 | 当前结论 |
| --- | --- |
| 当前运行驱动 | `3.3.3.42-patched-20` 已在本机重启并完成 PVR、Xorg/GLX、fbdev 和真实 VT 验收 |
| 发布判断 | patched-20 仅是本机历史验收物；除高频 `patch-008` 外，其 deb 还含所有权收敛前的旧 xdisplay 辅助载荷，禁止发布或在新设备部署 |
| 下一候选 | patched-21 已完成两次一致构建和离线包审计，SHA-256 为 `15c1fab4...1384cc`；未安装、未重启、运行待验收 |
| 唯一技术基线 | Deepin `20250421190503-debug` 完整原包；历史 patched 包不得作为后续载荷父版本 |
| 当前回退点 | patched-17；patched-8 只保留为更早的历史恢复物 |
| DRM/fbdev | `card0`、`renderD128`、`fb0` 可用，`/dev/fb0 mmap()` 和 `FBIOPAN_DISPLAY` 已通过 |
| 硬件 GL | renderer 为 `Fantasy II-M`，direct rendering、DRI3、GLX、Present 和加速通过 |
| Picom | patched v13 的运行时 shader 能力探测已验证；Picom 不进入显卡驱动 deb |
| 内置音频 | `1d94:14c9 -> snd_hda_intel -> SN6180`，ALSA/PipeWire 重启验证通过 |
| X11 显示管理 | xdisplay 由 dotconfig 仓库独立维护；本项目只提供 Innogpu 输出候选、模式恢复钩子和会话接入 |

当前可以确认的是“已安装的 patched-20 在本机工作”，不能据此宣称它已经具备跨设备发布条件。
该 deb 生成于 xdisplay 所有权收敛之前，不能用当前源码以相同版本号重建。下一候选必须使用大于 20
的新版本号，从 Deepin 202504 原包构建，移除或限速 PVR 诊断，并重复包边界、DKMS、固件、PVR、
Xorg/GLX、正常桌面和真实 VT 全部门槛。

patched-21 的精确定义、禁止载荷、构建证据格式和未来运行门槛见
[patched-21 release candidate](docs/patches/patched-21-release-candidate.md)。它已通过离线构建和包边界，
但仍需单独安装、重启并重新验收，不能继承 p20 的运行结论，也还不是新设备推荐版本。

## 从这里开始

| 需求 | 文档 |
| --- | --- |
| 了解当前版本、已解决和未解决问题 | [当前状态](docs/project/status.md) |
| 理解驱动、用户态和组件边界 | [项目架构](docs/project/architecture.md) |
| 在新设备上安装或准备回退 | [新设备安装](docs/user/new-device-install.md) |
| 安装或重启后逐项验收 | [状态验证](docs/user/verification.md) |
| 黑屏、Xorg 或驱动异常时恢复 | [故障恢复](docs/user/recovery.md) |
| 了解每个补丁的实际启用状态 | [阶段补丁](docs/patches/README.md) |
| 查看失败根因和排障经验 | [事故与经验](docs/incidents/README.md) |
| 修改代码、文档或 release | [维护策略](docs/project/maintenance-policy.md) |

## 新设备最小入口

从 release 下载外部包到 `debs/`。这些文件由 Git 忽略：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
debs/innogpu-fh2m_20250421190503-debug_amd64.deb
```

当前自动安装入口只面向 patched-17 回退/保守基线：

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
sudo scripts/install-patched17-and-check.sh
```

patched-20 只保留为当前机器的运行证据和回退起点，不再作为新设备安装选项。新候选的版本、补丁集
和验证计划确定前，不应直接调用通用构建器。详见[新设备安装](docs/user/new-device-install.md)和
[故障恢复](docs/user/recovery.md)。

## 组件所有权

### 驱动与用户态

本仓库拥有 Deepin 202504 coherent 构建器、内核补丁、设备恢复脚本、安装/回退入口和运行时探针。
DRI、GBM、GLAPI、GLVND、DDX、固件和 maintainer scripts 必须保持同源，禁止从 patched-8、17、18
或 19 中挑选单个文件拼入新包。

### X11 显示管理

xdisplay 引擎的唯一源码、配置、设计文档和测试位于 dotconfig 仓库：

```text
.local/bin/xdisplay
.local/bin/xdisplay.sh
.local/bin/displayselect
.local/lib/xdisplay/
.local/share/docs/project/display-device-adapter.md
.local/share/test/display/xdisplay-adapter.sh
```

本项目不复制这些文件，也不测试 xdisplay 的内部状态机。这里只维护：

```text
scripts/restore-dp1-mode-x11.sh
scripts/xdisplay-session.sh
scripts/install-xdisplay-user.sh
tests/xdisplay/run-install-tests.sh
```

其中安装器要求目标用户已经从 dotconfig 安装 xdisplay，然后只写入本设备的
`XDISPLAY_INTERNAL_OUTPUTS`、`XDISPLAY_RESTORE_COMMAND` 和带边界标记的 X11 会话接入。

当前已安装的 patched-20 deb 早于此边界，`/usr/share/innogpu-fh2m-trixie/` 中仍有未激活的旧
xdisplay 副本和旧安装器；它们不是当前源码，不得调用或发布。后续包由
`scripts/check-release-package.sh` 强制拒绝这些文件。

### Picom 与音频

patched Picom 使用独立源码和安装流程，见 [Picom 安装与恢复](docs/user/picom-install.md)。内置喇叭
由独立 PCI HDA 控制器提供，使用：

```sh
sudo scripts/install-hygon-hda-audio.sh
```

## 仓库结构

```text
.
|-- README.md             # 唯一文档入口和当前结论
|-- debs/                 # 本地 release 输入/输出；只跟踪 README
|-- patches/              # 可审查的内核与 Picom 补丁
|-- config/               # 项目拥有的配置模板
|-- scripts/              # 构建、安装、恢复、集成和诊断入口
|-- tools/                # 最小运行时探针和确定性的厂商对象变换工具
|-- tests/                # 本项目脚本及组件边界测试
|-- docs/
|   |-- project/          # 当前架构、状态和维护契约
|   |-- patches/          # 每个补丁的目的、启用状态和边界
|   |-- incidents/        # 失败证据、根因和经验
|   |-- planning/         # 历史、TODO 和挂起项
|   |-- user/             # 安装、验证、使用和恢复
|   `-- archive/          # 只读历史材料
|-- baselines/            # 精简历史证据，不替代当前运行检查
`-- third_party/          # 从外部 deb 生成，不进入 Git
```

完整文档阅读顺序见 [docs/README.md](docs/README.md)，脚本风险和生命周期见
[scripts/README.md](scripts/README.md)，手工探针和对象工具见 [tools/README.md](tools/README.md)。

## 维护底线

1. 后续候选只从完整 Deepin 202504 原包构建。
2. 当前、历史、候选和回退必须在文档中明确区分。
3. xdisplay 引擎变更只在 dotconfig 仓库进行，本项目只维护设备接入契约。
4. 外部 deb、原始日志、EDID、凭据、认证文件和本机绝对 home 路径不得提交。
5. 修改前先写目标、风险和回退；修改后运行对应测试并复核文档事实。
