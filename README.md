# innogpu-fh2m-debian-trixie

让 Innosilicon **Fantasy II-M（FH2M，风华2号-M）** GPU 在 **Debian Trixie (13)** 上稳定可用、
能力可知、可维护可演进的适配与验证项目：内核驱动、显示输出、硬件 GL、音频与全套验收证据。
本项目自有工作采用 GPL-3.0-or-later；fork 上游 MIT 内容与导入源码/厂商载荷按各自声明处理，
当前再分发边界见[许可证与再分发边界](docs/project/licensing.md)（唯一权威文档）。

> 最后更新：2026-09-02 —— 当前驱动包为 `4.0.1-i3`；R06 的首轮 s2idle 正常恢复，但候选 cursor 分支未执行，按停止条件终止 A/B。
> `4.0.1-i1` 已构建、安装并完成一次真实 s2idle 验证，但唤醒后外屏红屏，候选验收失败，禁止继续安装。
> R07 非挂起观测确认当前桌面 `cursor_enable=0`；patch-025 继续保持 UNVERIFIED，deep 继续冻结。

## 适配的当前系统

| 项 | 值 |
| --- | --- |
| 发行版 / 内核 | Debian Trixie (13)，kernel `6.12.101+deb13-amd64` |
| CPU 平台 | Hygon x86_64 |
| GPU | Innosilicon Fantasy II-M，PCI `1ec8:9810`，2 GiB VRAM（PowerVR DDK V119 RTM 谱系） |
| 当前驱动包 | `4.0.1-i3`：仅 patch-024；R06 首轮 s2idle 正常恢复，但 cursor treatment 未入组，不是稳定或修复结论 |
| 最近失败候选 | `4.0.1-i1`：在新架构构建中应用 patch-024；s2idle 唤醒后外屏红屏，已回退，不得安装或冒充当前运行版本 |
| 当前严格 A/B 候选 | `4.0.1-i3`：仅 patch-024；`4.0.1-i4`：024+025；同 epoch、包级单变量复验通过，但 R06 因 A 不复现且 cursor 分支未执行在 1/4 后停止 |
| 已验证能力 | Vulkan 1.3.264 枚举及队列提交 / OpenCL 3.0 枚举及 kernel 读回 / GL 4.3 core + GLES 3.2 / VA-API H.264 Main + HEVC Main 实际硬解（30 帧 320x240 NV12 输出校验）/ DMA-BUF 同设备 PRIME self-import + invisible GEM READ/WRITE + vblank 守卫 / DRM+fbdev / 桌面硬件 GL / HDA 与 PipeWire 枚举 |

## 版本演进

1. **fork 起步**：fork 自 [timhant/innogpu-fh2m-debian-trixie](https://github.com/timhant/innogpu-fh2m-debian-trixie)，初始为 kernel 6.12 兼容补丁（v3.3.3.42 基线）。
2. **适配本设备**：DPU/fbdev/connector/背光/GEM 系列修复（patched-8 → patched-27），本机稳定运行。
3. **迁移 Deepin**：以 Deepin 202504 完整原包为唯一技术基线，统一用户态/固件/DDX 载荷，消除 ABI 混配。
4. **完全重构（当前）**：取消 patch 叠加模式 → `drivers/` 仓库内维护的导入源码树 + manifest 管理黑盒，新构建器产出 `4.0.0-i1`（可复现构建；迁移阶段 0–4 完成，设备已运行）。
5. **suspend/resume 修复候选**：`4.0.1-i1` 因 s2idle 红屏失败；`i2` 一次恢复通过；严格定位在 `i3` 首轮发现 cursor 分支未执行后停止，R07 转为观测与触发条件设计。

## 主要修复的问题

- Debian 6.12 内核接口兼容（PCI resize API、Kbuild）；DP 启动 fbcon fallback
- fbdev `/dev/fb0` io mmap（ENODEV）；本机 connector/ACPI/eDP 映射
- invisible READ mapping 逐页回写缺陷；`dma_resv` usage 语义；未活动 CRTC vblank 守卫
- foreign DMA-BUF 生命周期；deb 构建可复现性（固定 epoch + 目录 mtime 归一化）
- deep resume 的 devfreq/PVR 电源状态竞态候选修复（patch-024，`4.0.1-i1` s2idle 可见恢复失败，deep 未测试）
- s2idle 红屏的 post-atomic 重复光标恢复候选修复（patch-025-suspend-resume-display；当前桌面 cursor 分支未入组，保持 UNVERIFIED）

补丁与事故详情见 [docs/patches/README.md](docs/patches/README.md)、[docs/incidents/README.md](docs/incidents/README.md)。

## 快速安装

```sh
git clone https://github.com/darkroam/innogpu-fh2m-debian-trixie.git && cd innogpu-fh2m-debian-trixie
sudo scripts/install-prereqs-debian.sh                        # 基础构建/运行依赖
# 最小化系统还须确认 python3、dpkg-deb 等新构建器直接命令（见 dependencies.md）
# 取得 Deepin 202504 原包放入 debs/（完整 SHA-256 见 docs/project/dependencies.md）
bash scripts/extract-vendor-binaries.sh                       # 按 manifest 重建 vendor/ 黑盒载荷
SOURCE_DATE_EPOCH=1788451200 bash scripts/build-innogpu-driver.sh  # 默认构建 4.0.1-i4
# A 对照：VERSION=4.0.1-i3 SOURCE_DATE_EPOCH=1788451200 bash scripts/build-innogpu-driver.sh
# 本轮只允许构建；安装和挂起验收须另开经审查轮次
```

> 新 clone 上 vendor/ 与 debs/ 均为空（不入库）：必须先取得 Deepin 原包并提取黑盒载荷，构建器
> 的 `--check-only` 门禁才会通过。

回退到保留基线：`sudo apt install --allow-downgrades ./debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb`
（详见 [docs/user/recovery.md](docs/user/recovery.md)）。

## 文档结构

| 目标 | 文档 |
| --- | --- |
| 推荐阅读顺序 / 完整索引 | [docs/README.md](docs/README.md) |
| 当前状态、风险、待办 | [docs/project/status.md](docs/project/status.md)、[docs/planning/current-work.md](docs/planning/current-work.md) |
| 已完成工作与时序 | [docs/planning/todo.md](docs/planning/todo.md) |
| 整体目标与路线 | [docs/project/goals.md](docs/project/goals.md) |
| 架构与组件边界 | [architecture.md](docs/project/architecture.md)、[code-analysis.md](docs/project/code-analysis.md) |
| 技术栈/参考模型 / 测试策略 | [frameworks-and-references.md](docs/project/frameworks-and-references.md)、[test-strategy.md](docs/project/test-strategy.md) |
| 新设备安装 / 验证 / 恢复 | [docs/user/new-device-install.md](docs/user/new-device-install.md)、[docs/user/verification.md](docs/user/verification.md)、[docs/user/recovery.md](docs/user/recovery.md) |
| 补丁与验收 / 事故 | [docs/patches/README.md](docs/patches/README.md)、[docs/incidents/README.md](docs/incidents/README.md) |
| 源码树迁移与 Phase 4/5 | [source-tree-migration.md](docs/planning/source-tree-migration.md)、[phase4](docs/planning/phase4-device-validation.md)、[phase5](docs/planning/phase5-retirement-design.md) |
| 脚本 / 工具 / 测试入口 | [scripts/README.md](scripts/README.md)、[tools/README.md](tools/README.md)、[tests/README.md](tests/README.md) |
| 多 Agent 协作 / 定期文档梳理 | [multiagent-collab.md](docs/project/multiagent-collab.md) |

## 致谢

- 上游仓库：[timhant/innogpu-fh2m-debian-trixie](https://github.com/timhant/innogpu-fh2m-debian-trixie)（本仓库 fork 来源）
- Deepin（V23 驱动载荷、打包结构与社区适配方案）
- Innosilicon（FH2M 硬件、驱动来源与固件；第三方许可按来源包和文件声明）
- Imagination Technologies / PowerVR（DDK V119 谱系与参照）
- [picom](https://github.com/yshui/picom)（上游合成器；本仓库维护 patched 构建）
- Debian 项目、DRM/KMS、Mesa、X.Org、PipeWire 等开源生态
- dotconfig（xdisplay 显示引擎，独立仓库维护）

## 许可证

- **原创层**（本项目后续原创的脚本、工具、测试、文档、配置和辅助工作）采用
  [GPL-3.0-or-later](LICENSE)；此前按 MIT 发布的版本及副本继续保有原 MIT 授权，换证不撤销
  既有授权。
- **上游继承层**：fork 自 [timhant/innogpu-fh2m-debian-trixie](https://github.com/timhant/innogpu-fh2m-debian-trixie)，
  Tim Hant 原始代码及其实质性派生内容继续保留 `Copyright (c) 2026 Tim Hant` 的 MIT 授权，全文
  见 [LICENSES/MIT.txt](LICENSES/MIT.txt) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
- **`drivers/`** 按逐文件声明处理：408 个文件映射为 `MIT OR GPL-2.0-only`，2 个文件映射为
  `BSD-3-Clause OR LGPL-2.1-only`；3 个 confidential 和 70 个无许可文件**排除出公开制品**，
  不继承根许可证。`OR` 只表示对应双许可文件的选择。
- 黑盒对象、用户态库、DDX、固件和其他载荷是本地取得的第三方内容，**不随公开制品发布**；
  `vendor-binary` 是来源分类，不是许可证名称。
- 许可边界（三层模型、阻断路径、制品范围）的唯一权威文档：
  [许可证与再分发边界](docs/project/licensing.md)；机械审计见
  [源码许可证审计](docs/project/source-license-audit.md)；第三方声明见
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
- 发布制品：`project-tools`（**候选制品**，机械门禁 `CLEARED`，当前不作为发布目标——发布决策 1C：按权利边界生成
  的允许清单，排除 patches/、debs/、collab/、drivers/、vendor/、build/、third_party/，非 drivers 逐路径
  分类 + NOTICE 门禁）与 `driver-source`（drivers/ 中仅明确许可文件，**非完整驱动**，`BLOCKED`）；
  **GitHub 主分支仍公开分发阻断路径，仓库级发布未闭环**；二进制 deb 与 vendor 载荷不作为
  当前发布目标。本地 `debs/` 与 `vendor/` 不参与发布。
