# 当前状态与问题清单

最后更新：2026-08-24

本文件是项目当前运行状态的唯一摘要。历史过程、补丁细节和故障推导分别见
[阶段补丁](../patches/README.md) 与 [事故和经验](../incidents/README.md)。

## 当前基线

| 项目 | 当前结论 | 证据 |
| --- | --- | --- |
| 当前运行驱动 | `4.0.0-i1`（迁移源码树 + manifest 黑盒载荷）已安装并重启至 `6.12.101+deb13-amd64`；A1–A12 实机验收全 PASS + p27 回退演练 PASS（2026-08-21 Phase 4） | [phase4 验收](../planning/phase4-device-validation.md)、[迁移设计](../planning/source-tree-migration.md) |
| 稳定图形历史基线 | 历史记录：`3.3.3.42-patched-21` 已安装、重启并完成本机 PVR、Xorg/GLX、fbdev、真实 VT、显示与 Picom 验收；不是当前运行包 | [`patched-21` 验收](../patches/patched-21-release-candidate.md) |
| 历史运行基线 | `3.3.3.42-patched-20` 曾完成运行验收，但 deb 含收敛前辅助载荷，仅保留为历史证据 | [`patched-20` 验收](../incidents/patched-20-runtime.md) |
| 包载荷边界 | 已验收 p20 deb 生成于 xdisplay 所有权收敛前，含旧引擎/实验辅助文件，不可发布或同版本重建 | [`patched-20` 载荷审计](../incidents/patched-20-legacy-helper-payload.md) |
| 历史运行验收 | p21 完整图形验收通过；p22 完成 connector 分类和开盖桌面烟测，但电源/合盖/拔屏矩阵未完成 | [`patch-009` 验收](../patches/patch-009-local-internal-edp-connector.md) |
| 源码/用户态基线 | Deepin 202504 完整原包，不混用历史 patched 包 | `scripts/build-innogpu-driver.sh`（新架构）；`build-deepin-coherent.sh` 为 legacy p27 oracle |
| 源码树迁移 | 阶段 0–4 完成（新构建器 4.0.0-i1 并行验证 + 实机候选验证与回退演练全 PASS）；设备已运行 4.0.0-i1；阶段 5 第一步（标记 deprecated + 文档同步）完成，第二步待条件满足 + 监督批准 | [phase5-retirement-design](../planning/phase5-retirement-design.md) |
| 固件与 PVR | `4.0.0-i1` 实机验收已确认固件加载，Driver/Firmware 为 OK，错误计数为 0 | [Phase 4 验收](../planning/phase4-device-validation.md) |
| DRM/fbdev | `4.0.0-i1` 实机验收已确认 `card0`、`renderD128`、`fb0` 可用；fbterm redraw 路径通过真实 VT 验证 | [Phase 4 验收](../planning/phase4-device-validation.md) |
| Xorg/GLX | 当前桌面和隔离 Xorg 的硬件加速验收通过 | [Phase 4 验收](../planning/phase4-device-validation.md) |
| 真实 VT | 普通用户 fbterm 可绘制和退出；禁用 YPan 后长输出、清屏及跨会话显示正常 | [`fbterm YPan 记录`](../incidents/fbterm-ypan-rendering.md) |
| 显示管理 | dotconfig 维护 xdisplay 2.0.0；本项目只维护设备钩子和会话接入 | [`display-management.md`](display-management.md) |
| Picom | patched v13 进程和 GLX 配置正在使用；最新 runtime 尚未独立确认实际 backend，保持 UNVERIFIED | [compositor-management.md](compositor-management.md)、[runtime 摘要](../../baselines/latest-runtime-baseline.txt) |
| 音频 | HDA/HDMI 声卡与 PipeWire 默认 sink 枚举正常，`aplay` 命令完成；最新受控听感确认仍为 UNVERIFIED | [audio-management.md](audio-management.md)、[runtime 摘要](../../baselines/latest-runtime-baseline.txt) |
| 能力验证工具 | `tests/runtime/run-capability-baseline.sh`（12 能力域、35 项、枚举/执行分离）；沙箱基线 15 PASS / 19 SKIP / 1 UNVERIFIED，合并 2026-08-24 真机证据后的权威摘要为 21 PASS / 9 SKIP / 5 UNVERIFIED | [runtime 摘要](../../baselines/latest-runtime-baseline.txt)、[tests/runtime/README](../../tests/runtime/README.md)、[test-strategy](test-strategy.md) |
| Vulkan/OpenCL 执行 | 探针 exec 模式 + 真机验证通过（2026-08-24）：Vulkan queue+fence submit+wait、OpenCL add kernel+读回逐元素校验均在 Fantasy II-M 上执行成功；`runtime_vulkan_execution`/`runtime_opencl_execution`=PASS（证据 `baselines/runtime-results-20260824.txt`）；离线失败路径测试 12 项 | [probe-vulkan-devices.c](../../tools/probe-vulkan-devices.c)、[probe-opencl-devices.c](../../tools/probe-opencl-devices.c)、[test-strategy](test-strategy.md) |
| VA-API 实际解码 | `tools/run-vaapi-decode-test.sh --codec all` 真机执行（2026-08-24）：H.264 Main 与 HEVC Main 强制 VA-API 硬解，各 30 帧 320x240 NV12 framemd5 与软件参考逐帧 hash 一致，Driver/Firmware 状态门禁通过；`runtime_vaapi_decode`=PASS（证据 `baselines/runtime-results-20260824.txt`）；能力边界仅 Main/Main 8-bit 4:2:0 | [run-vaapi-decode-test.sh](../../tools/run-vaapi-decode-test.sh)、[test-strategy](test-strategy.md) |

## 已解决问题

| 问题 | 修复/结论 | 阶段 |
| --- | --- | --- |
| Debian 6.12 与厂商内核接口不兼容 | 通过兼容补丁适配 DKMS 构建；已补充 6.12.101 的 PCI resize API 参数变化 | `patch-001` |
| DP 输出在启动阶段无安全 fallback | 当前候选启用 DP fbcon fallback | `patch-002` |
| 面板背光/平台注册和初始 enable 试验 | 历史补丁已保留，4.0.0-i1 沿用关闭这些实验补丁的稳定选择 | `patch-003` 至 `patch-005` |
| 本机 connector 与 ACPI 映射差异 | `patch-006` 修正 DPU 映射；`patch-009` 进一步把本机内置 DP0 在 hwinfo 失败时标记为 eDP | `patch-006`、`patch-009` |
| patched-17 的 `/dev/fb0 mmap()` 返回 `ENODEV` | `fb_mmap = fb_io_mmap`，p20 与 p21 均已通过真实 VT 验证 | `patch-007` |
| patched-18 用户态 ABI 混配 | 禁止从历史 deb 拼接用户态，统一以 Deepin 202504 原包重建 | 事故记录 |
| patched-18 缺少 shader 固件导致 PVR BAD | 从 Deepin 原包保留完整 `fh2m.fw/fh2m.sh/fh2c.fw/fh2c.sh` | coherent 构建及事故记录 |
| Picom 未声明 `GL_ARB_explicit_uniform_location` 而提前退出 | 运行时编译最小 shader 验证能力，成功后继续 | Picom patch |
| fbterm 快速滚动造成清屏错位和跨会话残留 | 增加可配置 redraw 模式及退出偏移复位，默认/16px 字体均通过真实 VT 验证 | fbterm 用户态补丁 |
| 显示引擎曾在本仓库形成重复副本 | 明确由 dotconfig 单独维护；本项目只注入 Innogpu 设备契约 | 显示集成阶段 |
| CPU_PREP 的 dma_resv usage 语义错误（bool 直接当 enum 传） | patch-025 用 `dma_resv_usage_rw()` 修正；patched-25 实机验证（PDP READ/WRITE 回归通过） | `patch-025` |
| 未活动 CRTC 的 vblank 请求成功返回后永久阻塞 | patch-026 拒绝无活动/无 mode 的 CRTC（返回 EINVAL）；patched-26 实机验证（CRTC 1 正常、CRTC 0/2 立即 EINVAL） | `patch-026` |
| foreign DMA-BUF 导入类型混淆、attach 错误未处理、GTT export 映射泄漏 | patch-027 增加 ops 检查、IS_ERR 处理与 unmap 配对；patched-27 实机验证（DRI3/PRIME 自导入回归正常） | `patch-027` |
| deb 构建不可复现（目录 mtime 未归一化） | release 审阅修复构建器（整树 mtime 归一化）；p25/26/27 重建为可复现 SHA | [release 审阅](../planning/release-review-2026-08-20.md) |

## 当前未解决或需要后续处理

1. p20 的 `patch-008` 历史诊断会对每次 services ioctl 写日志；p21 已关闭它，后续版本不得重新
   启用高频日志而不同时定义限速、证据和回退策略。
2. `hwinfo_g0m.bin` 仍缺失，但本次不阻止 PVR 进入 `ACTIVE`；是否需要该固件由后续硬件能力需求
   决定，不能仅凭缺失日志推断为故障。
3. 普通用户运行 `fbterm` 时不能修改内核键盘表，内置滚屏和切换 VT 快捷键不可用；这不是
   framebuffer 映射故障，不应直接授予全局特权。
4. 历史 p22 仅完成当前设备、当前内核的 connector/桌面烟测；电池合盖、外屏热插拔、不同扩展坞、
   三块及以上外屏、无盖桌面和多型号硬件的实机矩阵仍不完整。
5. p20 的旧显示安装器已随 p21 包升级移除；后续只使用当前仓库接入脚本与 dotconfig xdisplay。
6. xdisplay 的适配器、状态机、配置和自定义布局由 dotconfig 独立演进；本项目只需持续验证
   `XDISPLAY_INTERNAL_OUTPUTS`、`XDISPLAY_RESTORE_COMMAND` 和会话接入仍兼容。
7. patched-21 的实际 deb 哈希、离线包审计与完整运行证据已记录；patched-22 的实际 hash、connector
   烟测和重启证据已记录；patched-17 -> patched-23 回退恢复演练已通过，patched-24 的 6.12.101+
   DKMS 兼容和重启证据已记录，公开发布前仍需完成
   电源/合盖/拔屏矩阵、跨设备矩阵和 release 审阅。
8. 当前驱动仍报告 YPan 能力，但 stock fbterm 的加速滚动存在显示错位；用户态 redraw 已验证，
   内核侧应撤销能力声明还是修复平移语义尚未决定。
9. patched-22/`patch-009` 已修正本设备内置面板的 DRM connector 语义，历史重启测试观察到 `eDP-1` 和
   `Docked=false`；电池合盖、外屏接入/拔出和外部电源矩阵尚未全部完成。
10. FH2M invisible GEM 的只读 CPU mapping 在 VMA close 时无条件逐页执行 `SYS2GDDR`，已由独立
    PDP 探针复现并由 patched-23 修复；p23 实机验证通过，Clash Verge 启动态 A/B 已完成；调查、
    验收门槛与回退边界见 [`webkit-dmabuf-investigation.md`](../planning/webkit-dmabuf-investigation.md)。
11. p23 的 READ page fault 成本已完成 1/4/8/16 MiB 缩放测量，约按 `0.06–0.07ms/page` 增长；
    主要 DMA descriptor/wait 热点位于预编译 `innodma.o_shipped`，当前项目不制作 READ 预取候选。
12. `drivers/` 中存在 `Strictly Confidential`、BSD/LGPL 和引用缺失许可证文本的文件；第三方载荷
    也未完成逐项权利核实。关闭 [源码许可证审计](source-license-audit.md) 前，发布状态为 BLOCKED。

## 证据保留规则

- Git 只提交精简的 `result.txt`、摘要和文档，不提交原始 Xorg、GLX、EDID 或 trace 日志。
- 原始日志保留在本机主目录，文件名必须包含版本和日期，例如 `p20-kernel.log`。
- 每次新版本验收必须同时记录：包版本、完整载荷来源、固件加载、PVR 状态、Xorg/GLX 和真实 VT。

## 发布判断

patched-20、patched-21 和 patched-22 均为历史候选或验收证据，不是当前安装入口。p20 不得推广，
p21/p22 的电源、合盖、拔屏和跨硬件限制仍按历史记录保留。当前本地安装判断以 `4.0.0-i1`、
Phase 4 实机验收、`patched-27` 回退基线及 Phase 5 状态为准；公开发布则被许可证审计阻断。
`patched-17`/`patched-8` 仅作深层回退。
