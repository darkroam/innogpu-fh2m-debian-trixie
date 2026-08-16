# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上 Innosilicon Fantasy II-M / 风华 2 号 M 的驱动打包、兼容修复、
设备接入、硬件 GL、内置音频和验收项目。本文件是项目文档的唯一入口；当前结论以
[`docs/project/status.md`](docs/project/status.md) 为权威来源。

本项目面向维护特定 Innogpu 设备的操作者和开发者，不是可对任意显卡直接执行的通用驱动安装器。
首次接手应先阅读“当前状态与结论”和[术语表](docs/project/glossary.md)，再按新设备安装或故障恢复
文档操作；没有经过审阅的版本、外部 deb 或恢复通道时，不应直接运行构建或安装命令。

## 当前状态与结论

最后更新：2026-08-16。

| 项目 | 当前结论 |
| --- | --- |
| 当前运行驱动 | `3.3.3.42-patched-22` 已安装并重启；PVR/固件、DRM connector、RandR 和 xdisplay 桌面烟测通过，完整合盖矩阵待完成 |
| 稳定图形验收基线 | `3.3.3.42-patched-21` 已完成当前设备 PVR、Xorg/GLX、fbdev、真实 VT、显示与 Picom 验收 |
| 历史运行基线 | `3.3.3.42-patched-20` 曾完成验收，但其旧辅助载荷使该包不可发布 |
| 发布判断 | patched-20 仅是本机历史验收物；除高频 `patch-008` 外，其 deb 还含所有权收敛前的旧 xdisplay 辅助载荷，禁止发布或在新设备部署 |
| p21 验收状态 | 两次一致构建、离线包审计、部署与当前设备运行验收通过；跨硬件发布待评审 |
| 合盖策略版本 | patched-22 已从 Deepin 202504 构建并通过包边界检查，且已在当前设备安装、重启；内置面板已呈现为 DRM eDP，完整合盖/电源/拔屏矩阵待完成，哈希见 [patch-009](docs/patches/patch-009-local-internal-edp-connector.md) |
| 唯一技术基线 | Deepin `20250421190503-debug` 完整原包；历史 patched 包不得作为后续载荷父版本 |
| 当前回退链 | patched-22 -> patched-21 -> patched-17；patched-8 只保留为更早的历史恢复物 |
| DRM/fbdev | `card0`、`renderD128`、`fb0` 和 mmap 可用；fbterm 需使用已验证的 redraw 模式避开 YPan 显示错位 |
| 硬件 GL | renderer 为 `Fantasy II-M`，direct rendering、DRI3、GLX、Present 和加速通过 |
| Picom | patched v13 的运行时 shader 能力探测已验证；Picom 不进入显卡驱动 deb |
| 内置音频 | `1d94:14c9 -> snd_hda_intel -> SN6180`，ALSA/PipeWire 重启验证通过 |
| X11 显示管理 | xdisplay 由 dotconfig 仓库独立维护；本项目只提供 Innogpu 输出候选、模式恢复钩子和会话接入 |

当前可以确认的是“patched-22 已在当前设备启动并修正 connector 语义”。实测包版本为
`3.3.3.42-patched-22`，PVR/固件为 OK，内置面板从 DRM `DP-1` 变为 `eDP-1`，`Docked=false`，
RandR 与 xdisplay 均正常。p21 仍是完整图形验收基线；电池合盖、外屏接入/拔出和外部电源矩阵尚未在
p22 上全部完成。历史 p20 deb 生成于 xdisplay 所有权收敛之前，不能用当前源码以相同版本号重建或推广。

patched-21 的精确定义、禁止载荷、构建证据格式和未来运行门槛见
[patched-21 release candidate](docs/patches/patched-21-release-candidate.md)。它已完成当前设备验收，
但跨硬件 release 评审完成前仍不是无条件的新设备推荐版本。

## 版本选择

| 目标 | 应使用的版本 | 原因 |
| --- | --- | --- |
| 当前设备运行观察 | patched-22 | connector 语义和桌面烟测已通过；完整电源/合盖矩阵完成前不作为无条件发布版本 |
| 完整图形验收基线 | patched-21 | 当前设备、当前内核和当前显示组合已经通过完整验收 |
| patched-22 故障回退 | patched-21 | p21 是当前设备的完整图形验收回退点 |
| 新设备首次部署或 p21 故障回退 | patched-17 | 保守自动安装入口；跨硬件矩阵完成前不把 p21 作为默认部署包 |
| patched-17 仍无法启动时的历史恢复 | patched-8 | 更早的回滚物，不参与后续构建 |
| 查看历史诊断结论 | patched-20 | 只保留证据；旧辅助载荷使其禁止重新部署和发布 |

“当前设备已验收”不等于“已完成跨硬件发布”。新设备必须按保守入口部署，除非 p21 的跨硬件矩阵、
回退演练和 release 审阅均已完成。

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
| 查询术语和版本角色 | [术语表](docs/project/glossary.md) |
| 修改代码、文档或 release | [维护策略](docs/project/maintenance-policy.md) |

## 新设备最小入口

二进制 deb 不随 Git 发布。保守安装必须先取得经维护者审阅的 release 记录，并按其中的版本和
SHA-256 把以下两个包放入 `debs/`：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
```

没有该 release 记录时应停止并向维护者取得包，不能以未知来源 deb 或禁止部署的 patched-20 代替。
Deepin `20250421190503-debug` 原包只在构建后续 coherent 候选时需要，保守的 patched-17 安装不使用它。
完整的外部包角色、取得前提和安装步骤见[新设备安装](docs/user/new-device-install.md)。

当前自动安装入口只面向 patched-17 回退/保守基线：

```sh
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
sudo scripts/install-patched17-and-check.sh
```

patched-20 只保留为历史运行证据，不再作为新设备安装选项。新候选的版本、补丁集
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

历史 patched-20 deb 早于此边界，曾在 `/usr/share/innogpu-fh2m-trixie/` 携带未激活的旧 xdisplay
副本和旧安装器；它们不是当前源码，不得调用或发布。当前 p21 已移除这些文件，后续包由
`scripts/check-release-package.sh` 强制拒绝它们。

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
