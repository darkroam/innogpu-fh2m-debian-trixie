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
| `patches/` | 历史补丁 provenance 与新行为修复候选；已迁入源码树的补丁不重复叠加，R12 `4.0.2-i2` 绑定 patch-024 + patch-026-lifecycle + patch-028，R06 i3/i4 与失败的 R11 i1 保留为历史实验入口 |
| `components/picom/` | 当前维护的 Picom 源码补丁与项目配置模板（`001-probe-explicit-uniform-location.patch`、`picom.conf`） |
| `components/fbterm/` | 当前维护的 fbterm 用户态兼容补丁（`001-configurable-redraw-scrolling.patch`） |
| `debs/` | 本地 release/构建输入输出目录，`.deb` 被 Git 忽略，仅跟踪说明文件 |
| `scripts/` | 构建、安装、回退、诊断、显示接入、音频固化和验证入口 |
| `tools/` | 图形探针与聚合入口、确定性厂商对象变换、许可证/发布归档门禁、协作结构校验和共享隐私模式 |
| `tests/` | 本项目脚本、fbterm/Picom 和显示接入边界测试；xdisplay 引擎测试留在 dotconfig |
| `collab/` | dsh/codex 本机协作轮次；`.gitignore` 排除，不属于 Git 或公开分发面 |
| `docs/project/` | 架构、现状、依赖和维护边界 |
| `docs/patches/` | 每个代码补丁的独立设计、验证和回退说明 |
| `docs/incidents/` | 失败现场、根因推导、排除项和经验记录 |
| `docs/planning/` | 活动计划、历史、挂起项和迁移记录 |
| `docs/user/` | 新设备安装、日常验证、显示使用和故障恢复 |
| `docs/archive/` | 不再变化但仍有追溯价值的历史记录 |
| `baselines/` | 精简证据；`latest-runtime-baseline.txt` 是当前 35 项 runtime 汇总权威，其余文件按各自版本身份作为历史证据 |
| `third_party/` | 从外部 Deepin deb 生成的解包目录，不进入 git |

## 驱动与图形用户态

`innogpu-fh2m-trixie 4.0.0-i1` 是已完成基线验收、适配 Debian 6.12.101+ 的版本，并完成驱动、
DKMS、DRM/fbdev 与 A1–A12 实机验收的版本（迁移源码树 + manifest 黑盒载荷，见
[source-tree-migration.md](../planning/source-tree-migration.md)）；`patched-27` 转为保留的回退基线；
`patched-21` 是历史完整图形验收基线，`patched-17` 是深层回退包；它们不再是新设备默认入口。p25/p26/p27 分别增加 dma_resv usage 语义、未活动 CRTC vblank 守卫和 foreign DMA-BUF 生命周期修复，均已通过本机实机验收（见 [patch-025](../patches/patch-025-dma-resv-usage-rw.md)、[patch-026](../patches/patch-026-inactive-crtc-vblank-guard.md)、[patch-027](../patches/patch-027-foreign-dmabuf-lifecycle.md)）。p20 deb 是所有权收敛前的历史运行证据，包内辅助
脚本不能代表当前源码，禁止重新部署或发布；运行时验收与 release 载荷合规是两个独立结论。当前
已安装的 `4.0.0-i1` 直接维护 `drivers/` 源码，并从固定 Deepin 202504 原包按 manifest 提取完整
同源用户态 ABI、固件和黑盒对象；其历史补丁不在构建时叠加。失败候选 `4.0.1-i1` 确定性
应用 patch-024，s2idle 可见恢复已失败。`4.0.1-i2` 保留 patch-024 并增加
patch-025-suspend-resume-display，R05 已完成一次 s2idle 可见恢复；R06 改用 i3/i4 做严格包级单变量
  对照，R10 随后在 i3 上复现 deep PowerLock TOCTOU 并回退 `4.0.0-i1`。R11 的 `4.0.2-i1`
  应用 patch-024 + lifecycle 026，但独立温度 work 仍在 PVR 恢复前触发并导致 deep 失败。
  R12 的 `4.0.2-i2` 再增加 patch-028，等待全部 PVR 子设备与 DVFS 恢复成功；两者都不含
  display 025，且 i2 尚未安装。后续每次行为变化仍必须独立升号。patched-21 使用
`patch-006`；patched-22
另加 `patch-009`，patched-23 再加 `patch-023`，patched-24 增加 `patch-001` 的 6.12.101+ API
兼容，这些版本关系仅用于历史 provenance。connector 修复仅针对本机
DPU match 141 的内置 DP0/eDP 语义，invisible GEM 修复只跳过 READ staging 页的回写。
`patched-8` 只保留为更早的历史回滚物。

Deepin 202504 deb 同时提供硬件 GL/DDX 用户态。内核模块成功、DRM 节点存在和 Xorg 出图不能
单独证明硬件加速可用；必须分别验证 renderer、direct rendering、DRI、GLX 和 Present。

### 驱动包构建基线

当前 `4.0.0-i1` 构建由 `scripts/build-innogpu-driver.sh` 统一编排：直接复制 Git 跟踪的
`drivers/` 源码树，从 `vendor/` 取得 `binary-manifest.json` 校验过的黑盒对象、固件和用户态载荷，
再使用本项目已审查的 Debian maintainer scripts 生成包。`vendor/` 由
`scripts/extract-vendor-binaries.sh` 从固定 SHA-256 的 Deepin 202504 原包幂等重建，不进入 Git。

当前构建器默认接受 `4.0.2-i2` 与固定 epoch `1788710400`：在 patch-024 后应用
patch-026-suspend-resume-dvfs-lifecycle 与 patch-028，不含 UNVERIFIED 的 display 025。补丁同时
应用到离线编译 staging 和最终包内 DKMS 源码，并拒绝 `.orig/.rej/.o.cmd` 产物。失败的
`4.0.2-i1`（epoch `1788624000`）与 R06 `4.0.1-i3/i4` 仍可显式复现；更早 i1/i2 不再由当前
源码复用。当前运行版本是回退后的 `4.0.0-i1`。

Deepin 原包仍是导入源码、用户态 ABI 和黑盒载荷的唯一技术来源。9 个历史启用补丁已转换为
`drivers/` 中的源码提交；新行为修复必须先以独立补丁和升号候选验证。patch-024 的 i1
真机验收未通过；patch-025-suspend-resume-display 的 i2 单次恢复通过，但严格因果验证仍使用
i3/i4 对照；R11 lifecycle 026 用 Debian 6.12 devfreq 的同步 suspend 语义关闭 devfreq 并发源，
但 R11 deep 揭示独立温度 work 的第二条入口；R12 patch-028 为此增加 PVR 子设备恢复门禁，仍需
真机 deep 验收。旧
`build-deepin-coherent.sh` 和 patched wrapper 仅作为 p27 oracle、历史复现和回退证据保留。
禁止从任一历史 patched 包复制用户态文件、对象或控制脚本后再局部替换源码。

因此：

- `drivers/` 是当前可修改源码权威，`binary-manifest.json` 是第三方载荷路径与哈希权威；
- Deepin 202504 原包是二者的来源基线，DRI、GBM、GLAPI、GLVND 和 DDX 禁止跨版本混配；
- `patched-8` 只是历史回滚点，`patched-17` 是一次不延续 patched-8 实现谱系的重建，两者都不是
  后续版本的实现父版本；
- `patched-18` 的 fbdev 补丁本身已通过 mmap 验证，但其旧构建流程错误地复用了 patched-8 的包载荷
  和控制脚本，因此不能作为后续打包基线；
- `patched-19` 是第一个执行完整 Deepin 202504 载荷规则的历史候选，关键 vendor 文件哈希和 DRI
  未解析符号检查已通过；
- `patched-20` 在完整 Deepin 202504 载荷上保留 `fb_io_mmap` 并加入 PVR 初始化诊断，已通过运行时
  PVR、隔离 Xorg/GLX 和真实 VT `fbterm` 验收；但其 deb 还包含收敛前的旧显示/实验辅助文件，
  只能作为当前机器证据，不能发布或用当前源码以相同版本号重建；
- 历史 patched 包和 patch wrapper 不得作为新版本实现父版本。

当前构建必须显式提供经过审阅的 `SOURCE_DATE_EPOCH`，并通过 manifest `--check-only`、
`check-release-package.sh`、oracle/符号对比和可复现双构建门禁。旧 `build-deepin-coherent.sh` 仍保留
历史版本号拒绝与 epoch 护栏，但不是新候选的构建入口。

`4.0.0-i1` 的包内路径分为三层：manifest 导入的 vendor 载荷按映射安装（其中
`sw-inno-gl.service` 保留为 `/lib/systemd/system/sw-inno-gl.service`，`sw-inno-gl` 安装到
`/usr/sbin/`）；项目维护的 12 个 helper 实体安装到 `/usr/share/innogpu-fh2m-trixie/`；其中 10 个
稳定命令同时从 `/usr/bin/innogpu-*` 与 `/usr/sbin/innogpu-*` 链接到实体。包的 `postinst` 只写入
`/etc/modprobe.d/innogpu.conf`、执行 DKMS build/install、校验 coherent DRI、运行
`ldconfig`/`depmod`/`update-initramfs`，不会 enable 或 start `sw-inno-gl.service`。因此“文件随包存在”
不能写成“服务已启用”。当前 release gate 也尚未覆盖该 vendor unit/helper 组合及全部 10 个命令链接，
该缺口记录在 `code-analysis.md` 与 `../planning/current-work.md`。

patched-21 是 coherent 历史规则的首个实际输出：离线包边界、重复构建和当前设备运行验收均已通过，
但尚未完成跨硬件发布。它的固定补丁矩阵和证据见
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
- `baselines/latest-runtime-baseline.txt` 是当前 runtime 汇总权威，但其 `tested_commit` 与证据边界必须
  保留；其他 `latest-*` 只有在文件名或内容带版本身份时才能证明对应版本。改变当前机器前仍需本地
  运行时验证，任何摘要都不能替代授权后的设备检查。

## 修改顺序

每次行为修改遵循：先更新对应文档为“计划”，再改代码并验证，最后把文档状态复核为“已生效”或
记录未通过项。完整要求见 `maintenance-policy.md`。
