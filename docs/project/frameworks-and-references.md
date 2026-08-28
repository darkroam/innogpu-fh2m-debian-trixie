# 开发框架、技术栈与参考模型（4.0.0-i1 基线）

> 2026-08-21 首次产出。记录本项目"基于什么开发"的事实：实际使用的框架、
> 版本基线、参考模型、证据与约束。结论等级：`CONFIRMED`（源码/脚本/清单）、`OBSERVED`（实机/
> 日志）、`INFERRED`（机制推断）、`UNVERIFIED`（未证实）。不把黑盒观察当作源码事实。

## 一、实际技术栈（CONFIRMED，来自代码/构建脚本）

| 层 | 组件 | 使用方式 | 证据 |
| --- | --- | --- | --- |
| 内核模块 | Linux kernel module + Kbuild + DKMS | `drivers/` 由 DKMS（dkms.conf: innogpu-kernel 2.2）编译；Makefile/Kbuild 直编 | drivers/dkms.conf、Makefile |
| 内核子系统 | DRM/KMS、fbdev、GEM、DMA-BUF、PCI、ALSA、dma_resv/fence | 驱动源码直接使用（inno_drm.c、inno_mm.c、innodpu_*、inno_audio.c 等） | drivers/innogpu/*、innosrvkm/* |
| 用户态图形 | X.Org modesetting、Mesa GLX/EGL/GBM、DRI2/DRI3、GLVND、VA-API | 驱动包携带 Deepin 同源用户态（libVK_INNO/libINNOOCL/GL-MESA/GBM/DRI/DDX）；Xorg 用 modesetting+innogpu DRI | manifest userspace 条目、check-desktop-hwgl 输出（AIGLX innogpu） |
| 计算 API | Vulkan 1.3.264（DRIVER_ID_IMAGINATION_PROPRIETARY）、OpenCL 3.0 | 预编译 ICD（libVK_INNO、libINNOOCL） | capability-survey.md（OBSERVED） |
| 桌面/服务 | X11+dwm、Picom（patched）、PipeWire/WirePlumber、systemd、udev、initramfs | 项目维护 Picom patched 构建与设备钩子；音频由 PipeWire 管理 | scripts/build-patched-picom.sh、compositor-management.md |
| 构建/测试 | Bash（67 脚本）、Python 3（tools/）、dpkg/apt、DKMS、shell fixture | 构建器/清单/提取器/校验全为 Bash+Python；测试为 shell fixture | scripts/、tools/、tests/ |

## 二、来源基线与参考模型（事实/证据/限制）

| 参考对象 | 关系 | 版本/来源 | 证据与限制 | 结论等级 |
| --- | --- | --- | --- | --- |
| fork 来源 `timhant/innogpu-fh2m-debian-trixie` | 直接 fork 来源 | GitHub（最早提交 8be37ed） | README 致谢；仓库无 fork 元数据 | CONFIRMED(名称)/UNVERIFIED(URL 细节) |
| Deepin 202504 原包 | **源码基线 + 用户态载荷来源 + 打包参考（三者同时）** | deb SHA `b5a70e78…f6f5b2` | drivers/ 导入、manifest 192 项、coherent 打包规则 | CONFIRMED |
| Innosilicon/FH2M 厂商内容 | 导入源码、黑盒对象、固件和用户态 | 随 Deepin 包 | 源码头包含 Dual MIT/GPL 引用、BSD/LGPL 和 `Strictly Confidential`；黑盒不可读；逐项许可 UNVERIFIED | CONFIRMED(存在与头部)/UNVERIFIED(再分发权) |
| PowerVR DDK V119 RTM | 谱系/参考 | BVNC 35.V.1632.23、G0M_SOC、META MTP219 | ddk-v119-mapping.md；与主线 drm/imagination + Mesa pvr 对照 | OBSERVED(特征)/INFERRED(谱系) |
| Linux DRM/Mesa/PVR 主线 | **行为/语义对照**（非直接依赖） | 内核 6.12.101 | ddk-v119-mapping、patch 兼容层（drm_edid_block_valid stub 等） | INFERRED(用途)/CONFIRMED(存在) |
| Picom（yshui/picom） | 外部组件 + 本项目 patched 构建 | commit `6d676824c457a933c52e3e92c5a1856466f90545` 固定 | build-patched-picom.sh、GLX 扩展缺失 patch | CONFIRMED |
| fbterm（Debian 1.7-5） | 外部组件 + patched 构建 | Debian 源 | components/fbterm/ 补丁、redraw 模式 | CONFIRMED |
| dotconfig xdisplay | 集成目标（独立仓库） | 不固定在本仓库 | 仅设备钩子/会话接入/恢复命令；引擎不入库 | CONFIRMED |
| X.Org/modesetting/Mesa | 运行时集成目标 | 系统包 | AIGLX innogpu、DRI2 driver innogpu（OBSERVED） | CONFIRMED/OBSERVED |

## 三、关键问题的结论

1. **drivers/ 是否仍依赖旧 patch 叠加？** 否——已是**直接编译的源码树**（CONFIRMED：
   build-innogpu-driver.sh 无 patch 应用；9 个补丁已转提交）。
2. **Deepin 的角色？** 三者同时：源码基线（drivers/ 导入）、用户态载荷来源（manifest 192 项）、
   打包参考（coherent 规则）。（CONFIRMED）
3. **DRM/Mesa/PVR 参考代码是否真被使用？** 仅**语义/行为对照**与兼容桩（如 drm_edid_block_valid
   stub、dma_resv 适配）；不直接构建上游驱动。（INFERRED 用途 + CONFIRMED 桩存在）
4. **Picom/fbterm/dotconfig 的关系？** Picom/fbterm = 外部组件，本项目维护 patched 构建；
   dotconfig = 集成目标（独立仓库）。（CONFIRMED）
5. **4.0.0-i1 vs patched-27 oracle：哪些是包内容相等，哪些是运行行为相等？**
   - 包内容相等（CONFIRMED，compare-oracle-candidates 全 PASS）：control/file_list/payload_hashes/
     dkms_source_parity/blackbox_hashes/maintainer_scripts/module_symbols（vermagic/符号表）。
   - 运行行为相等（OBSERVED，Phase 4 A1-A12 两版本分别验收 PASS）：Driver/Firmware、HWGL、vblank、
     PDP、DRI3 自导入、音频/显示。
6. **参考模型风险？** Picom commit 固定（CONFIRMED 无漂移）；Deepin 包 SHA 固定；DRM/Mesa 主线
   对照结论多属 INFERRED（机制推断），不冒充源码事实；黑盒对象不可验证（UNVERIFIED 许可/内部）。

## 四、约束汇总

- 许可证：本项目原创层使用根 [LICENSE](../../LICENSE)（GPL-3.0-or-later），fork 上游 MIT 继承层
  保留 `Copyright (c) 2026 Tim Hant`；`drivers/` 不是统一许可证，confidential 文件再分发权、
  70 个 `NOASSERTION` 路径及黑盒/用户态/固件逐项许可均未核实，confidential 与无许可路径
  **排除出公开制品**；标准条款副本已补齐但不解决授权链，发布状态 BLOCKED
  （[`source-license-audit.md`](../project/source-license-audit.md)）。
- 黑盒边界：不修改 .o_shipped/用户态/固件；仅字节契约（gpupll 变换）与符号级可观察。
- 版本限制：4.0.0-i1 为当前；patched-27 仅回退/oracle；版本号禁止复用。
- 不可复现部分：设备实测、黑盒内部、逐项许可结论。

## 证据索引

`docs/planning/ddk-v119-mapping.md`、`capability-survey.md`、`reverse-engineering-assessment.md`、
`docs/project/licensing.md`、`docs/project/source-license-audit.md`、`binary-manifest.json`、`drivers/dkms.conf`、
`scripts/build-innogpu-driver.sh`。
