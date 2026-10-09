# 阶段补丁

每个补丁文件只描述一个可独立审查的变更。源码 diff 位于仓库 `patches/`；stage-000 因目标是厂商
预编译对象，使用 `tools/internal/patch-gpupll-object.py` 执行严格字节契约。本目录记录各阶段的目的、应用
条件、验证证据和回退边界。Deepin 与 fantgpu 分线使用各自锁定来源，不得混配；
当前版本角色与执行冻结见 [status](../project/status.md)。

## fantgpu 030 链

`5.0.0-i6` 的 O_stage 已含 030 链 18 项，编号非连续，不能把 `030-001..034` 当作 34 项。
逐项对应以 [030 映射表](../planning/030-mapping-table.md)、
[O_stage 设计](../design/o-stage-integration-plan.md) 和
[i6 meta](../planning/evidence/o-stage/5.0.0-i6/5.0.0-i6.meta.json) 为准。
030-032 探针破坏 shipped ABI、030-033 改用独立 devres 的经过见
[i4 Oops 事故](../incidents/r5-i4-oops-dev-rsrc-abi.md)。ABI 修正不解除 R5=FAIL；
OUTSIDE_COVERAGE、禁止重跑、U1/U2 未执行、validation-results 未签、未打 tag 均保持。

`030-036` 是 i12 终树 `9a8d185f2a65` 之后的续编，不进入 i12 构建器，因此不改锁定终树门。
它只把 `fantgpu/fant_math.h` 的私有 `__bf_shf` 改名为 `__fant_bf_shf`，移位表达式仍是
`(__builtin_ffsll(x) - 1)`。见 [030-036](030-036-bf-shf-rename.md)。

**以下分类为 Deepin 补丁与组件的历史记录，计数不含 030 链**：

- `patches/*.patch`（18 个源码 diff：001–009、023–029，以及显示恢复候选
  `025-suspend-resume-display.patch` 和 lifecycle 候选
  `026-suspend-resume-dvfs-lifecycle.patch`、温度 work 候选
  `028-suspend-resume-hal-temp-monitor-delay.patch`、DDCCI panel 候选
  `029-suspend-resume-ddcci-panel.patch`；另有 stage-000 确定性工具）——其中 13 个
  历史补丁已在源码树迁移时转为 `drivers/` 内的提交，不再重复叠加；新增 patch-024 是
  `4.0.1-i1` 的独立实验修复，由新架构构建器确定性应用，但 s2idle 可见恢复验收失败。本表保留 provenance、事故证据与
  legacy 回退包复现依据。
- `components/picom/`、`components/fbterm/`——**当前维护的第三方组件补丁与配置**（2026-08-21 由
  `patches/picom/`、`patches/fbterm/`、`config/` 迁入，历史内容保留）：补丁由
  `tools/build-patched-picom.sh`、`tools/build-patched-fbterm.sh` 在构建对应组件时应用，
  与驱动包构建无关；`components/picom/picom.conf` 是项目维护的配置模板，由
  `tools/install-picom-user.sh` 作为默认配置源安装。

## 当前第三方补丁台账

截至 2026-10-02，仓库中**当前维护的第三方组件补丁共 4 个**。这里的“第三方”按
**补丁修改的上游/厂商来源、许可和版本基线**判定，不按是否进入 DKMS 判定。DKMS 只是
内核外部模块的构建/打包机制；一个补丁即使修改内核源码而不进入 DKMS，仍然可以是第三方
补丁；反过来，DKMS 里的本项目修改也不因此成为第三方补丁。

| 路径 | 上游/目标 | 适用基线 | 类型与当前状态 |
| --- | --- | --- | --- |
| `components/fbterm/001-configurable-redraw-scrolling.patch` | Debian fbterm | 1.7-5 | 用户态第三方派生补丁；真实 VT 验证通过 |
| `components/linux/001-hygon-148c-xhci-reset-on-resume.patch` | Linux xHCI PCI | Debian `linux-source-6.12` `6.12.107-1` | 内核第三方派生补丁；107 基线已部署。111 已按该版本源码 no-fuzz 重编并验收 quirks=`0x90`，未复用 107 模块。其他内核版本仍须重验，不得直接套用 |
| `components/linux/002-kaitian-x7h-battery-notification-delay.patch` | Linux ACPI battery | Debian `linux-source-6.12` `6.12.107-1` | 机型专用内核 workaround；107 基线已安装并一轮拔插通过。111 已按该版本源码 no-fuzz 重编，第 2 轮拔插 BAT0 保持。其他内核版本不得直接套用 |
| `components/picom/001-probe-explicit-uniform-location.patch` | picom | 固定上游 commit `6d676824c457a933c52e3e92c5a1856466f90545` | 用户态第三方派生补丁；实机通过 |

每个第三方补丁必须同时记录：目标组件、上游版本或 commit、目标文件、许可/NOTICE、适用
内核或组件版本、应用方式、静态/运行验证、当前安装状态和升级时的重新验证要求。机器权威
登记位于 [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md) 与
[`license-audit-policy.json`](../../license-audit-policy.json)；事故或运行问题另见
[`docs/incidents/`](../incidents/README.md)。

## 与 `patches/`、DKMS 的边界

- `components/`：当前维护的第三方组件派生补丁；每个新增文件必须进入上述台账和许可审计。
- `patches/`：Deepin/fantgpu 阶段补丁、迁移补丁、诊断候选和历史实验。它们按项目血缘、
  目标树和许可状态单独记录，**不因位于仓库且不在 DKMS 中就自动算第三方**；其中部分已
  迁入 `drivers/`，部分是混合/未决许可并排除出公开制品。
- `drivers/` 与 DKMS：这是厂商驱动源码和其构建/打包路径。是否第三方取决于源码来源和
  许可声明，不取决于 DKMS 这个包装机制。
- Linux `components/linux/*.patch`：修改发行版内核源码，不会因为 DKMS 自动应用，也不会随普通
  内核升级自动保留。长期部署优先从当前系统包管理器获取与目标内核匹配的源码包；当前 Debian
  实现使用 `apt-get download linux-source-6.12=<version>`，源码 `.deb` 保存在本地 `debs/`，
  不安装成系统源码包。

### 部署前置条件

| 补丁 | 需要的源码/包 | 不需要时的处理 |
| --- | --- | --- |
| fbterm-001 | fbterm 1.7-5 源码或对应 Debian 源码包，以及 fbterm 构建依赖；不需要内核 headers | 不使用 fbterm 或不需要该 redraw 行为时不构建、不安装 |
| picom-001 | 固定 picom commit 的完整源码，以及 picom 构建依赖；不需要内核 headers | 不使用 patched picom 时不构建、不安装 |
| Linux xHCI reset-on-resume | 包管理器提供的匹配源码包、目标内核 headers/build tree、`CONFIG_USB_XHCI_PCI=m` 和构建依赖 | 不使用该内核候选或设备不需要该 Hygon quirk 时不应用；普通 DKMS 构建不会自动接入 |
| Linux ACPI battery workaround | 包管理器提供的匹配源码包、目标内核 headers/build tree、`CONFIG_ACPI_BATTERY=m` 和构建依赖 | 不使用该机型电池 workaround 时不应用；普通 DKMS 构建不会自动接入 |

Linux 两个补丁是“内核源码补丁”，不是 DKMS 模块补丁。若发行版已经提供包含同等修改的内核
包，直接安装该包即可；否则从包管理器取得匹配的完整源码包并在证据目录临时展开。当前 107
配置中两个目标均为模块，因此长期部署只重编 `battery.ko` 和 `xhci-pci.ko`，不重建 `bzImage`。
源码包提供目标 `.c`，headers/build tree 提供已安装内核的生成头、符号和 `vermagic`；两者不能
相互替代。

源码 `.deb` 持久保存在 `debs/`；每次构建重新展开干净工作树。工作树、构建输出及安装证据位于
本轮 `.build/` 证据目录，审查后可只清理工作树。源码 `.deb` 只有在对应内核版本不再保留且用户
明确执行清理时才删除。入口为 `build-linux-patched-modules.sh`、
`install-linux-patched-modules.sh`、`rollback-linux-patched-modules.sh`、
`cleanup-linux-module-worktree.sh` 和 `prune-kernel-source-cache.sh`。

脚本对未知包管理器、源码版本不匹配、补丁 fuzz、非模块配置及 `vermagic` 不匹配 fail-closed。
未来 RPM 系统应接入其包管理器的源码包，不得把 Debian 包名硬套到其他发行版。

`components/linux/001-hygon-148c-xhci-reset-on-resume.patch` 也属于第三方派生补丁：它修改
的是上游 Linux xHCI PCI 驱动，而不是 DKMS 的 InnoGPU 驱动。目标设备若不需要该 quirk，或使用
的内核已内置同等修复，就不应重复应用。

## 历史内核和驱动补丁

| 阶段 | 代码补丁 | 构建开关/入口 | 状态 |
| --- | --- | --- | --- |
| 000 | [skip-first-gpupll](patch-000-skip-first-gpupll.md) | 始终应用 | patched-19 至 p27 启用；4.0.0-i1 由确定性工具继续应用 |
| 001 | [kernel-6.12](patch-001-kernel-6.12.md) | 始终应用 | patched-19 至 p27 启用；4.0.0-i1 源码树已包含，p24+ 实机适配 6.12.101+ |
| 002 | [dp-fbdev-fallback](patch-002-dp-fbdev-fallback.md) | `APPLY_DP_FBCON_FALLBACK=1` | patched-19 至 p27 启用；4.0.0-i1 源码树已包含 |
| 003 | [panel-backlight-fallback](patch-003-panel-backlight-fallback.md) | `APPLY_PANEL_BACKLIGHT_FALLBACK=1` | 历史验证；当前关闭 |
| 004 | [panel-platform-fallback](patch-004-panel-platform-fallback.md) | `APPLY_PANEL_PLATFORM_FALLBACK=1` | 历史验证；当前关闭 |
| 005 | [backlight-initial-enable](patch-005-backlight-initial-enable.md) | `APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=1` | 历史验证；当前关闭 |
| 006 | [local-connector-acpi-map](patch-006-local-connector-acpi-map.md) | `APPLY_LOCAL_CONNECTOR_ACPI_MAP=1` | patched-19 至 p27 启用；p21 已在当前设备运行验收，后续版本继承；4.0.0-i1 源码树已包含 |
| 007 | [fbdev-io-mmap](patch-007-fbdev-io-mmap.md) | `APPLY_FBDEV_IO_MMAP=1` | patched-19 至 p27 启用并实机通过；4.0.0-i1 源码树已包含 |
| 008 | [pvr-init-diagnostic](patch-008-pvr-init-diagnostic.md) | `APPLY_PVR_INIT_DIAGNOSTIC=1` | 仅 patched-20 诊断启用 |
| 009 | [local-internal-edp-connector](patch-009-local-internal-edp-connector.md) | `APPLY_LOCAL_INTERNAL_EDP=1` | patched-22 至 p27 继承；connector/桌面烟测通过，4.0.0-i1 源码树已包含；电源与合盖矩阵待完成 |
| 023 | [invisible-read-no-writeback](patch-023-invisible-read-no-writeback.md) | `APPLY_INVISIBLE_READ_NO_WRITEBACK=1` | patched-23 至 p27 继承并实机通过；4.0.0-i1 源码树已包含；Clash 启动态 A/B 已完成 |
| 024 | [suspend-resume](024-suspend-resume.md) | 新架构 `4.0.1-i1` 固定应用；legacy `APPLY_SUSPEND_RESUME_FIX=1` | 构建/启动门禁通过；真实 s2idle 无 PowerLock 错误但外屏红屏，候选失败并回退；deep 未测试 |
| 025-display | [suspend-resume-display](025-suspend-resume-display.md) | R06 B=`4.0.1-i4` 在 patch-024 后固定应用；A=`4.0.1-i3` 不应用 | 去除 atomic state replay 后的重复光标恢复；i3/i4 包级单变量通过，但首轮 A 未复现且 cursor 分支未入组，A/B 已停止，保持 UNVERIFIED。编号与历史 025 并存，以完整文件名区分 |
| 025 | [dma-resv-usage-rw](patch-025-dma-resv-usage-rw.md) | `APPLY_DMA_RESV_USAGE_FIX=1` | patched-25 至 p27 继承并实机通过；4.0.0-i1 源码树已包含 |
| 026-lifecycle | [suspend-resume-dvfs-lifecycle](026-suspend-resume-dvfs-lifecycle.md) | 新架构 `4.0.2-i1/i2` 在 patch-024 后固定应用 | drain devfreq target 后再 PVR 下电、PVR 上电成功后再恢复 devfreq；i1 deep 失败，i2 继续继承。编号与历史 026-vblank 并存，以完整文件名区分 |
| 026 | [inactive-crtc-vblank-guard](patch-026-inactive-crtc-vblank-guard.md) | `APPLY_INACTIVE_CRTC_VBLANK_GUARD=1` | patched-26/p27 实机通过；4.0.0-i1 源码树已包含；活动/未活动 CRTC 回归通过 |
| 027 | [foreign-dmabuf-lifecycle](patch-027-foreign-dmabuf-lifecycle.md) | `APPLY_FOREIGN_DMABUF_LIFECYCLE_FIX=1` | patched-27 实机验证安装/HWGL/DRI3 自导入；4.0.0-i1 源码树已包含；foreign/跨设备路径仍未实机触发 |
| 028 | [suspend-resume-hal-temp-monitor-delay](028-suspend-resume-hal-temp-monitor-delay.md) | 新架构 `4.0.2-i2/i3` 在 patch-024 + lifecycle 026 后固定应用 | 等待全部 PVR 子设备及 DVFS 恢复成功后再启动温度 work；i3 已通过 R14 6/6 deep |
| 029 | [suspend-resume-ddcci-panel](029-suspend-resume-ddcci-panel.md) | 新架构 `4.0.2-i3` 在 patch-024 + lifecycle 026 + patch-028 后固定应用 | DDCCI 回退模式创建 panel，但不注册 backlight device；i3 已通过 R14 6/6 deep 并正式交付 |

patched-24 不增加新的设备行为补丁；它沿用 patched-23 的补丁集合，并把 patch-001 的
`6.12.101+` PCI API 兼容修复打包进去，供新内核 headers 的 DKMS 自动构建使用。

## 用户态补丁

| 阶段 | 代码补丁 | 状态 |
| --- | --- | --- |
| Picom-001 | [explicit-uniform-location](picom/patch-picom-001-explicit-uniform-location.md) | 实机通过 |
| fbterm-001 | [configurable-redraw-scrolling](../incidents/fbterm-ypan-rendering.md) | 真实 VT 验证通过 |

## 构建顺序

**Deepin 源码树线（历史交付、当前回退 `4.0.2-i3`；`4.0.0-i1` 为其首层回退；`4.0.2-i1` 已失败）**：

```text
Deepin 202504 原 deb
  -> tools/build-innogpu-driver.sh（drivers/ 源码树 + 版本绑定补丁 + manifest 黑盒 + 确定性变换）
  -> 4.0.1-i3：patch-024；4.0.1-i4：patch-024 + patch-025-suspend-resume-display（历史实验）
  -> 4.0.2-i1：patch-024 + patch-026-suspend-resume-dvfs-lifecycle（R11 deep 失败）
  -> 4.0.2-i2：i1 + patch-028-suspend-resume-hal-temp-monitor-delay（历史未安装候选）
  -> 4.0.2-i3：i2 + patch-029-suspend-resume-ddcci-panel（R14 6/6 deep 正式交付）
  -> 离线 DKMS 编译 + 完整包组装
  -> tools/check-release-package.sh
```

**历史 patched 包（legacy，保留）**：

```text
Deepin 202504 原 deb
  -> 新版本号（必须 >20）和已审查补丁开关
  -> tools/build-deepin-coherent.sh
       -> stage-000 GPU PLL 对象变换和 patch-001 始终应用
       -> wrapper 显式选择 patch-002 至 patch-007
       -> tools/check-release-package.sh
  -> 打包、DKMS、固件完整性检查
  -> 隔离 Xorg/GLX
  -> 重启后 PVR/DRM/fbdev/fbterm
```

不要把某一阶段的 `.so`、固件或 maintainer script 从另一个 patched 包复制到当前构建中。
历史 patched-8、patched-17、patched-18、patched-19 只可作为证据或回退物，不能作为后续载荷父版本。
表中的“历史验证”不等于当前候选启用；复现具体包时以对应 wrapper 的环境变量为准。

patched-19/20 的固定 wrapper 已改为拒绝执行，因为当前源码的辅助载荷边界与原历史 deb 不同；继续
使用相同版本号会制造“版本相同、包内容不同”的不可审计产物。表中的 p19/20 开关集合只记录当时
实际启用的驱动补丁，不表示当前可以重建同名包。

## 版本候选

- [patched-21：所有权收敛后的首个 release candidate](patched-21-release-candidate.md)：固定启用
  stage-000、patch-001/002/006/007，关闭 patch-003/004/005/008；分开记录构建、包边界与运行
  验收。p21 已完成当前设备运行验收，仍不能继承 p20 的包或运行证据，也尚未完成跨硬件发布。
- patched-22：`tools/build-patched22-local-lid.sh` 固定启用 patch-009，已从 Deepin 202504
  原包构建、通过包边界检查并在当前设备重启；它只修正本机内置 DP0/eDP 语义，电源与合盖实机矩阵仍待完成。
- patched-23：`tools/build-patched23-invisible-read-fix.sh` 在 p22 开关集合上只增加 patch-023，修复
  invisible READ mapping 释放时的无意义回写；历史上已安装、重启并完成基础图形与 Clash 启动态 A/B，
  当前只作 provenance/回退链证据。
- patched-24：`tools/build-patched24-kernel-612101.sh` 沿用 p23 全部开关，增加 Debian
  `6.12.101` 及以后 headers 的 `pci_resize_resource(..., exclude_bars)` 兼容分支；构建和安装前
  必须重新执行对应内核的 DKMS 编译验证；2026-08-18 已重启并确认 p24、DKMS、Driver/Firmware
  和 DRM/fbdev 正常。
- patched-25：`tools/build-patched25-dma-resv-fix.sh` 增加 patch-025（CPU_PREP 的 dma_resv
  usage 语义修复）；已实机验证并合并、打 tag。
- patched-26：`tools/build-patched26-vblank-guard.sh` 增加 patch-026（未活动 CRTC vblank 守卫）；
  已实机验证并合并、打 tag。
- patched-27：`tools/build-patched27-foreign-dmabuf.sh` 增加 patch-027（foreign DMA-BUF 生命周期
  修复）；已实机验证并合并、打 tag。
- patched-28：`tools/build-patched28-suspend-resume.sh` 继承 p27 并增加 patch-024（resume 早期
  devfreq 电源状态门禁）；补丁编号 024 是空缺回填，包版本不复用历史 patched-24；仅作 legacy
  对照。新架构 `4.0.1-i1` 的 s2idle 可见恢复验收已失败，不再作为可安装候选。
- `4.0.1-i2`：R05 完成一次 s2idle 可见恢复；是历史候选，不作为严格 A/B 包复用。
- `4.0.1-i3/i4`：R06 严格 A/B 对照，共用 epoch `1788451200`；i3 仅 patch-024，i4 再加
  patch-025-suspend-resume-display。包级单变量准备通过；i3 首轮未复现且 cursor 分支未入组，
  后续 deep 在 i3 上复现 PowerLock TOCTOU，机器已回退 `4.0.0-i1`。
- `4.0.2-i1`：R11 失败候选，固定 epoch `1788624000`；只叠加 patch-024 与 lifecycle 026。
  deep 恢复时温度 work 仍提前触发 PowerLock/POWERED_OFF，保留历史复现入口但不得安装或交付。
- `4.0.2-i2`：R12 历史静态/离线候选，固定 epoch `1788710400`；在 i1 上增加 patch-028，
  不含 UNVERIFIED 的 display 025。不得作为当前安装候选，deep 必须另获批准。
- `4.0.2-i3`：Deepin 历史交付、当前回退基线，固定 epoch `1788796800`，SHA-256
  `177133eebda692092501a27d7d135662ddaedaf3634776b8aa1ea5153c9e1662`；在 i2 上增加
  patch-029，让 DDCCI 回退模式创建 panel 但不注册 backlight device；不含 display 025。
  R14 已完成接电/电池、无外屏/外屏 6/6 deep，结论仅覆盖当前设备与该矩阵。
- p25/26/27 的 deb 均为可复现构建（[release 审阅](../archive/release-review-2026-08-20.md) 修复
  目录 mtime 后重建），SHA 见 [debs/README.md](../../debs/README.md)。
