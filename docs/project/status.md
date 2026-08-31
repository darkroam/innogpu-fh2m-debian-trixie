# 当前状态与问题清单

最后更新：2026-08-31

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
| 能力验证工具 | `tests/runtime/run-capability-baseline.sh`（12 能力域、35 项、枚举/执行分离）；沙箱基线 15 PASS / 19 SKIP / 1 UNVERIFIED，合并真机证据后的权威摘要为 22 PASS / 9 SKIP / 4 UNVERIFIED | [runtime 摘要](../../baselines/latest-runtime-baseline.txt)、[tests/runtime/README](../../tests/runtime/README.md)、[test-strategy](test-strategy.md) |
| 维护协作 | dsh 负责监督/审查，codex 负责实现；每 5–6 轮或重大调整后执行两阶段文档梳理；`collab/` 仅本机保存、不进 Git | [多 Agent 协作规约](multiagent-collab.md) |
| Vulkan/OpenCL 执行 | 探针 exec 模式 + 真机验证通过（2026-08-24）：Vulkan queue+fence submit+wait、OpenCL add kernel+读回逐元素校验均在 Fantasy II-M 上执行成功；`runtime_vulkan_execution`/`runtime_opencl_execution`=PASS（证据 `baselines/runtime-results-20260824.txt`）；离线失败路径已有 fixture | [probe-vulkan-devices.c](../../tools/probe-vulkan-devices.c)、[probe-opencl-devices.c](../../tools/probe-opencl-devices.c)、[test-strategy](test-strategy.md) |
| VA-API 实际解码 | `tools/run-vaapi-decode-test.sh --codec all` 真机执行（2026-08-24）：H.264 Main 与 HEVC Main 强制 VA-API 硬解，各 30 帧 320x240 NV12 framemd5 与软件参考逐帧 hash 一致，Driver/Firmware 状态门禁通过；`runtime_vaapi_decode`=PASS（证据 `baselines/runtime-results-20260824.txt`）；能力边界仅 Main/Main 8-bit 4:2:0 | [run-vaapi-decode-test.sh](../../tools/run-vaapi-decode-test.sh)、[test-strategy](test-strategy.md) |
| DMA-BUF 回归工具 | `tools/run-dmabuf-regression-test.sh` 已实现（2026-08-24）：同设备 PRIME self-import + invisible GEM READ/WRITE + vblank 守卫 + 状态门禁聚合，配套离线 fixture；**真机 PASS（2026-08-26 root 权限运行，证据已封存）**：self-import/READ/WRITE/vblank/状态门禁/内核日志全部通过；能力边界不变：仅同设备 PRIME self-import，foreign/cross-device、GBM、V4L2、长期压力与并发仍 UNVERIFIED | [run-dmabuf-regression-test.sh](../../tools/run-dmabuf-regression-test.sh)、[test-strategy](test-strategy.md)、[webkit 调查](../planning/webkit-dmabuf-investigation.md) |
| 发布边界 | 三层许可模型（原创层 GPL-3.0-or-later / 上游 MIT / drivers/ 逐文件）；`project-tools` 为**候选制品**（机械门禁 CLEARED，当前不作为发布目标；**失败关闭分类**——已批准原创前缀 + 显式映射，未知路径拒绝，无默认 GPL；排除 patches/、debs/、collab/（本机私有目录，不跟踪）、drivers/、vendor/、build/、third_party/；**路径绑定 NOTICE 门禁**，components/ 许可材料已封存：picom 补丁为文件级 MPL-2.0、`picom.conf` 为原创 GPLv3、fbterm 1.7-5 (C) 2008 dragchan GPL-2.0-only）；`driver-source` 排除 confidential ×3 与无许可 ×70 后非完整驱动（BLOCKED，不假 PASS）；**GitHub 主分支仍公开分发阻断路径，仓库级发布未闭环**；二进制 deb 与 vendor 载荷不作为当前发布目标；patched-1.deb 为上游历史非阻断；本地 debs/ 与 vendor/ 不参与发布；**发布决策 1C（见 licensing.md §4.1 权威记录）：当前不创建 Release/tag/附件，main 为研究开发仓库、不作为发布目标，BLOCKED 不变；不做 Release 不消除 main 公开跟踪 73 个阻断路径的风险** | [licensing.md](licensing.md)（唯一权威文档）、[source-license-audit.md](source-license-audit.md) |

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
   DKMS 兼容和重启证据已记录。电源/合盖/拔屏和跨设备矩阵作为研发验证继续；
   只有未来明确推翻 1C 时才重新激活 release 审阅。
8. 当前驱动仍报告 YPan 能力，但 stock fbterm 的加速滚动存在显示错位；用户态 redraw 已验证，
   内核侧应撤销能力声明还是修复平移语义尚未决定。
9. patched-22/`patch-009` 已修正本设备内置面板的 DRM connector 语义，历史重启测试观察到 `eDP-1` 和
   `Docked=false`；电池合盖、外屏接入/拔出和外部电源矩阵尚未全部完成。
10. FH2M invisible GEM 的只读 CPU mapping 在 VMA close 时无条件逐页执行 `SYS2GDDR`，已由独立
    PDP 探针复现并由 patched-23 修复；p23 实机验证通过，Clash Verge 启动态 A/B 已完成；调查、
    验收门槛与回退边界见 [`webkit-dmabuf-investigation.md`](../planning/webkit-dmabuf-investigation.md)。
11. p23 的 READ page fault 成本已完成 1/4/8/16 MiB 缩放测量，约按 `0.06–0.07ms/page` 增长；
    主要 DMA descriptor/wait 热点位于预编译 `innodma.o_shipped`，当前项目不制作 READ 预取候选。
12. 许可证体系最终整理（三层模型，2026-08-28 监督复审前）：原创层 GPL-3.0-or-later、上游
    MIT 继承层（`Copyright (c) 2026 Tim Hant`）、drivers/ 逐文件声明、本地载荷排除；
    `project-tools` 为**候选制品**（CLEARED 仅机械门禁）：按权利边界生成允许清单（排除
    patches/、debs/、collab/、drivers/、vendor/、build/、third_party/），非 drivers **失败关闭**分类 +
    路径绑定 NOTICE 门禁；components/ 许可材料已封存（picom 补丁为文件级 MPL-2.0、配置为
    原创 GPLv3，fbterm 1.7-5 (C) 2008 dragchan GPL-2.0-only）；`driver-source` 仅含明确许可文件（BLOCKED，
    非完整驱动）；3 个 confidential 与 70 个
    无许可路径**排除出公开制品**（不从 Git 历史删除）；GitHub 主分支仍分发阻断路径，仓库级
    发布未闭环；机械审计 `license_audit_overall=PASS`、`license_release_gate=BLOCKED`，见
    [源码许可证审计](source-license-audit.md) 与 [licensing.md](licensing.md)。
13. 运维子项（第一批，2026-08-28 已修复并加 fixture）：DRI repair 安装器统一 helper 实际安装路径与
    unit `ExecStart`（包 `/usr/sbin` vs 源码 fallback `/usr/local/sbin` 明确区分）、`enable/start`
    失败传播（不再 `|| true` 冒充成功）、卸载器安全删除源码 fallback 创建的 helper 符号链接（只删
    指向本项目脚本的链接，不删用户/包文件）；`check-soft-xorg-dwm.sh` 删除固定 `USER_NAME=ok`，用户
    解析统一 `INNOGPU_X_USER > SUDO_USER > USER`、home 用 `INNOGPU_X_HOME`/`getent` 且不可确定时明确
    失败（不再回退 root `$HOME`），`run_x` 使用同一解析用户；`try-hotload-patched17.sh` 提示改为中性
    （不再“log in as ok”）；VA-API runner 三个阶段都把 GNU timeout rc=124/137 归类为超时（整体退出
    码 5），新增忽略 TERM→SIGKILL fixture；新增 `tests/unit/run-dri-repair-tests.sh`（helper
    路径/ExecStart/PATH 注入忽略/失败回滚只删本次新建/外国文件拒绝/版本不匹配零副作用/package-absent
    只清 DRI 自有路径/幂等安装卸载/不删除非本项目文件 + 无硬编码用户名静态
    反例）。`check-docs.sh` 已覆盖全部 tracked Markdown 和本机 `collab/` 隐私扫描。仍在待办：音频安装器对称卸载与 fixture、构建依赖门禁、
    vendor `sw-inno-gl.service` 生命周期，见 [代码分析](code-analysis.md) 与
    [当前待办](../planning/current-work.md)。

## 证据保留规则

- Git 只提交精简的 `result.txt`、摘要和文档，不提交原始 Xorg、GLX、EDID 或 trace 日志。
- 原始日志保留在本机主目录，文件名必须包含版本和日期，例如 `p20-kernel.log`。
- 每次新版本验收必须同时记录：包版本、完整载荷来源、固件加载、PVR 状态、Xorg/GLX 和真实 VT。

## 发布判断

patched-20、patched-21 和 patched-22 均为历史候选或验收证据，不是当前安装入口。p20 不得推广，
p21/p22 的电源、合盖、拔屏和跨硬件限制仍按历史记录保留。当前本地安装判断以 `4.0.0-i1`、
Phase 4 实机验收、`patched-27` 回退基线及 Phase 5 状态为准；公开发布则被许可证审计阻断。
`patched-17`/`patched-8` 仅作深层回退。

**发布决策 1C（当前状态，2026-08-28）**：

- 当前**不创建 GitHub Release、tag 或发布附件**；`main` 继续作为研究开发仓库。
- `license_release_gate=BLOCKED` **保持不变**；`project-tools=CLEARED` 仍只表示候选制品机械门禁
  通过，`driver-source=BLOCKED` 保持不变。
- **不得声称“不做 Release”可以消除 `main` 当前公开跟踪 3 个 Strictly Confidential + 70 个无许可
  路径（共 73 个）的风险**——分支本身仍是公开分发面；其处置保留为独立发布决策。
