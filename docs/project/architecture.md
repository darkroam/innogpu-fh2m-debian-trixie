# 项目架构

## 目的

本文面向维护者，说明 Innosilicon Fantasy II-M 在 Debian Trixie 上的驱动、显示、图形用户态、
音频和安装脚本之间的职责边界。日常安装与恢复见 `../user/`，事故经验见 `../incidents/`，阶段补丁见
`../patches/`，未完成工作见 `../planning/`。开发和重构必须遵守 `maintenance-policy.md` 中的不可变规则。

## 总体链路

```text
当前安装：4.0.0-i1 deb
  -> scripts/build-innogpu-driver.sh -> dpkg
       -> DKMS + 同源 Deepin 202504 DDX/GL/固件
       -> /dev/dri/card*、renderD*、/dev/fb0
       -> X11 会话中的 dotconfig `xdisplay watch`
            -> lid + DRM connector + RandR
            -> 显示布局与热插拔收敛
       -> patched Picom GLX
            -> 圆角、模糊、动画和窗口合成

PCI 0000:06:00.6 [1d94:14c9]
  -> hygon-hda-audio.service
  -> snd_hda_intel -> Conexant SN6180
  -> ALSA -> PipeWire/WirePlumber
```

## 目录职责

| 路径 | 职责 |
| --- | --- |
| `drivers/` | Deepin 202504 DKMS 源码树及已转换为提交的 9 个启用修复 |
| `patches/` | 历史内核补丁原件、转换 provenance 和补丁说明；当前源码不再通过 patch 叠加构建 |
| `components/picom/` | 当前维护的 Picom 源码补丁与项目配置模板（`001-probe-explicit-uniform-location.patch`、`picom.conf`） |
| `components/fbterm/` | 当前维护的 fbterm 用户态兼容补丁（`001-configurable-redraw-scrolling.patch`） |
| `debs/` | 本地 release/构建输入输出目录，`.deb` 被 Git 忽略，仅跟踪说明文件 |
| `scripts/` | 构建、安装、回退、诊断、显示接入、音频固化和验证入口 |
| `tools/` | EGL、GBM、X11、loader 的最小探针，以及确定性的厂商对象变换工具 |
| `tests/` | 本项目脚本、fbterm/Picom 和显示接入边界测试；xdisplay 引擎测试留在 dotconfig |
| `docs/project/` | 架构、现状、依赖和维护边界 |
| `docs/patches/` | 每个代码补丁的独立设计、验证和回退说明 |
| `docs/incidents/` | 失败现场、根因推导、排除项和经验记录 |
| `docs/planning/` | 活动计划、历史、挂起项和迁移记录 |
| `docs/user/` | 新设备安装、日常验证、显示使用和故障恢复 |
| `docs/archive/` | 不再变化但仍有追溯价值的历史记录 |
| `baselines/` | 精简后的 pass/fail 历史证据，不作为当前运行状态来源 |
| `third_party/` | 从外部 Deepin deb 生成的解包目录，不进入 git |

## 驱动与图形用户态

`innogpu-fh2m-trixie 4.0.0-i1` 是当前设备已安装并重启、适配 Debian 6.12.101+ 并完成驱动、
DKMS、DRM/fbdev 与 A1–A12 实机验收的版本（迁移源码树 + manifest 黑盒载荷，见
[source-tree-migration.md](../planning/source-tree-migration.md)）；`patched-27` 转为保留的回退基线；
`patched-21` 是历史完整图形验收基线，`patched-17` 是深层回退包；它们不再是新设备默认入口。p25/p26/p27 分别增加 dma_resv usage 语义、未活动 CRTC vblank 守卫和 foreign DMA-BUF 生命周期修复，均已通过本机实机验收（见 [patch-025](../patches/patch-025-dma-resv-usage-rw.md)、[patch-026](../patches/patch-026-inactive-crtc-vblank-guard.md)、[patch-027](../patches/patch-027-foreign-dmabuf-lifecycle.md)）。p20 deb 是所有权收敛前的历史运行证据，包内辅助
脚本不能代表当前源码，禁止重新部署或发布；运行时验收与 release 载荷合规是两个独立结论。后续包统一
以 Deepin 202504 原包为唯一技术基线，在其 DKMS 源码上叠加 Debian 6.12 兼容、G0M PLL、DRM/fbdev
和本地 connector/invisible GEM 修复，并保留同一原包中的完整用户态 ABI 集合。patched-21 使用
`patch-006`；patched-22 另加 `patch-009`，patched-23 再加 `patch-023`，patched-24 增加
`patch-001` 的 6.12.101+ API 兼容。connector 修复仅针对本机
DPU match 141 的内置 DP0/eDP 语义，invisible GEM 修复只跳过 READ staging 页的回写。
`patched-8` 只保留为更早的历史回滚物。

Deepin 202504 deb 同时提供硬件 GL/DDX 用户态。内核模块成功、DRM 节点存在和 Xorg 出图不能
单独证明硬件加速可用；必须分别验证 renderer、direct rendering、DRI、GLX 和 Present。

### 驱动包构建基线

后续候选包必须直接解包 `innogpu-fh2m_20250421190503-debug_amd64.deb`，以其中的 DKMS 源码、
固件、预编译对象、DRI、GBM、GLAPI、GLVND、Xorg DDX 和相互链接关系作为同一个不可拆分的基线，
然后只在 Deepin DKMS 源码上按版本应用仓库补丁，并用本项目已审查的 Debian maintainer scripts
替换上游安装脚本。禁止从任一历史 patched 包复制用户态文件或控制脚本后再局部替换源码。

因此：

- Deepin 202504 原包是后续版本唯一的源码、用户态 ABI 和打包载荷基线；
- `patched-8` 只是历史回滚点，`patched-17` 是一次不延续 patched-8 实现谱系的重建，两者都不是
  后续版本的实现父版本；
- `patched-18` 的 fbdev 补丁本身已通过 mmap 验证，但其旧构建流程错误地复用了 patched-8 的包载荷
  和控制脚本，因此不能作为后续打包基线；
- `patched-19` 是第一个执行完整 Deepin 202504 载荷规则的历史候选，关键 vendor 文件哈希和 DRI
  未解析符号检查已通过；
- `patched-20` 在完整 Deepin 202504 载荷上保留 `fb_io_mmap` 并加入 PVR 初始化诊断，已通过运行时
  PVR、隔离 Xorg/GLX 和真实 VT `fbterm` 验收；但其 deb 还包含收敛前的旧显示/实验辅助文件，
  只能作为当前机器证据，不能发布或用当前源码以相同版本号重建；
- DRI、GBM、GLAPI、GLVND 和 DDX 必须来自同一 Deepin 发布，禁止跨版本混配或只替换其中一个文件。

当前 `build-deepin-coherent.sh` 只接受显式指定且大于 20 的新版本号，以及经过审阅的
`SOURCE_DATE_EPOCH`。固定 wrapper 必须声明两者，使相同源码和输入重复构建得到逐字一致的 deb。
构建完成后必须通过 `check-release-package.sh`，确认关键 ABI/固件存在、设备接入脚本与源码一致，
并拒绝 xdisplay 引擎副本、Kylin/实验用户态安装器和直接二进制热补丁命令。

patched-21 是该规则的首个实际输出：离线包边界、重复构建和当前设备运行验收均已通过，但尚未完成
跨硬件发布。它的固定补丁矩阵和证据见
[`../patches/patched-21-release-candidate.md`](../patches/patched-21-release-candidate.md)。

## 显示管理

显示布局属于 X11 会话，而不是 DKMS 内核模块。目标所有权是：

```text
Xorg/RandR
  -> xprofile 启动 dotconfig 的 xdisplay watch
       -> 自动布局、热插拔、开合盖、stale 输出清理

dotconfig 的手动入口 displayselect
  -> 与 watcher 共用同一 DISPLAY 的 apply lock
```

通用引擎不得硬编码外屏名称、数量或分辨率。引擎源码、库、配置、命令和内部测试由 dotconfig
独立维护。本项目只通过 `XDISPLAY_INTERNAL_OUTPUTS`、`XDISPLAY_RESTORE_COMMAND` 和
`restore-dp1-mode-x11.sh` 注入本设备的非标准内屏候选与模式恢复动作；
`install-xdisplay-user.sh` 只安装这些接入文件，不复制或覆盖 dotconfig 的显示引擎。当前契约见
[`display-management.md`](display-management.md)，历史吸纳记录见
[`../planning/display-integration.md`](../planning/display-integration.md)。

## 音频

`innogpu` 注册的 `InnosiliconCard` 是 DP/HDMI 数字音频。本机内置喇叭来自独立 PCI HDA 控制器
`1d94:14c9`，需要显式绑定 `snd_hda_intel`，codec 为 Conexant SN6180。

系统服务负责驱动绑定，用户服务在 PipeWire/WirePlumber 启动后恢复默认 sink 和 mixer。不得全局
导出 `ALSA_CONFIG_PATH` 指向用户配置，因为该变量会替换系统 ALSA 配置并破坏设备 profile 枚举。

## X11 合成器

Picom 使用 Innogpu 硬件 GLX。驱动的 GLSL 编译器能够编译 explicit uniform location，但扩展字符串
没有声明 `GL_ARB_explicit_uniform_location`，上游 Picom 因此在创建 backend 前提前退出。项目
补丁只在最小 shader 实际编译成功时继续，编译失败仍保持上游拒绝行为，不伪造其他 GL 能力。

这是本设备图形用户态的特例边界：能力判断由下游 Picom 的运行时 shader 探测承担，不通过修改
预编译 Innogpu `.so` 或增加显卡型号白名单解决。该探测不能移入通用显示脚本，也不能视为所有
Innogpu 或其他 GPU 的默认能力；驱动用户态、Picom 基线或 GLSL 编译器变化后必须重新验证。

Picom 属于独立用户态组件，不进入显卡驱动 deb。项目固定上游提交、保存 patch、配置和会话启动
片段；源码仍从上游单独 clone。Picom 未安装时会话片段可回退到 `xcompmgr`。

## framebuffer 终端

驱动包负责提供可映射的 `/dev/fb0` 和准确的 fbdev 能力；fbterm 是独立用户态组件，不进入驱动 deb。
当前驱动的 mmap 已通过，但其 YPan 快速滚动与 fbterm 1.7 的偏移管理不兼容，因此本项目保存
`components/fbterm/` 补丁和独立构建入口（`scripts/build-patched-fbterm.sh`），以
`scrolling=redraw` 保证正确显示。该规避不改变 framebuffer
可见/虚拟尺寸，也不代表驱动 YPan 已修复；根因、对照证据和回归门槛见
[`../incidents/fbterm-ypan-rendering.md`](../incidents/fbterm-ypan-rendering.md)。

## 配置边界

- 仓库保存可复用脚本、补丁、模板、验证和必要的设备适配逻辑。
- 活动 `/etc/X11`、logind、udev 状态必须先记录和审查，不能从本机直接整份复制进仓库。
- 用户完整 `xprofile` 包含输入法、Picom、代理等无关设置，不能整体吸纳；只提取显示启动契约。
- 外部 `.deb`、原始日志、EDID、序列号、用户名和绝对 home 路径不得提交。
- `baselines/latest-*` 是历史证据；改变当前机器前仍需本地运行时验证。

## 修改顺序

每次行为修改遵循：先更新对应文档为“计划”，再改代码并验证，最后把文档状态复核为“已生效”或
记录未通过项。完整要求见 `maintenance-policy.md`。
