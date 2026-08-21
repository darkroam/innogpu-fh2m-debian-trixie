# innogpu-fh2m-debian-trixie

让 Innosilicon **Fantasy II-M（FH2M，风华2号-M）** GPU 在 **Debian Trixie (13)** 上稳定可用、
能力可知、可维护可演进的开源适配项目：内核驱动、显示输出、硬件 GL、音频与全套验收证据。

> 最后更新：2026-08-21 —— 当前驱动包 `4.0.0-i1`（源码级重构版），已在本机完成全套实机验收与回退演练。

## 适配的当前系统

| 项 | 值 |
| --- | --- |
| 发行版 / 内核 | Debian Trixie (13)，kernel `6.12.101+deb13-amd64` |
| CPU 平台 | Hygon x86_64 |
| GPU | Innosilicon Fantasy II-M，PCI `1ec8:9810`，2 GiB VRAM（PowerVR DDK V119 RTM 谱系） |
| 当前驱动包 | `4.0.0-i1`：自有驱动源码树 `drivers/` + `binary-manifest.json` 管理的黑盒载荷 |
| 已验证能力 | Vulkan 1.3.264 / OpenCL 3.0 / GL 4.3 core + GLES 3.2 / VA-API H264+HEVC 硬解 / DRM+fbdev / 桌面硬件 GL / HDA 音频 |

## 版本演进

1. **fork 起步**：fork 自 [timhant/innogpu-fh2m-debian-trixie](https://github.com/timhant/innogpu-fh2m-debian-trixie)，初始为 kernel 6.12 兼容补丁（v3.3.3.42 基线）。
2. **适配本设备**：DPU/fbdev/connector/背光/GEM 系列修复（patched-8 → patched-27），本机稳定运行。
3. **迁移 Deepin**：以 Deepin 202504 完整原包为唯一技术基线，统一用户态/固件/DDX 载荷，消除 ABI 混配。
4. **完全重构（当前）**：取消 patch 叠加模式 → `drivers/` 自有源码树 + manifest 管理黑盒，新构建器产出 `4.0.0-i1`（可复现构建；迁移阶段 0–4 完成，设备已运行）。

## 主要修复的问题

- Debian 6.12 内核接口兼容（PCI resize API、Kbuild）；DP 启动 fbcon fallback
- fbdev `/dev/fb0` io mmap（ENODEV）；本机 connector/ACPI/eDP 映射
- invisible READ mapping 逐页回写缺陷；`dma_resv` usage 语义；未活动 CRTC vblank 守卫
- foreign DMA-BUF 生命周期；deb 构建可复现性（固定 epoch + 目录 mtime 归一化）

补丁与事故详情见 [docs/patches/README.md](docs/patches/README.md)、[docs/incidents/README.md](docs/incidents/README.md)。

## 快速安装

```sh
git clone <本仓库> && cd innogpu-fh2m-debian-trixie
sudo scripts/install-prereqs-debian.sh                        # 构建/运行依赖
SOURCE_DATE_EPOCH=1787342400 bash scripts/build-innogpu-driver.sh  # 构建 4.0.0-i1
sudo apt install ./build/innogpu-fh2m-trixie_4.0.0-i1.deb     # 安装（同包名升级）
sudo reboot                                                    # 重启后加载新模块
```

回退到保留基线：`sudo apt install --allow-downgrades ./debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb`
（详见 [docs/user/recovery.md](docs/user/recovery.md)）。

## 文档结构

| 目标 | 文档 |
| --- | --- |
| 推荐阅读顺序 / 完整索引 | [docs/README.md](docs/README.md) |
| 当前状态、风险、待办 | [docs/project/status.md](docs/project/status.md)、[docs/planning/todo.md](docs/planning/todo.md) |
| 整体目标与路线 | [docs/project/goals.md](docs/project/goals.md) |
| 架构与组件边界 | [docs/project/architecture.md](docs/project/architecture.md) |
| 新设备安装 / 验证 / 恢复 | [docs/user/new-device-install.md](docs/user/new-device-install.md)、[docs/user/verification.md](docs/user/verification.md)、[docs/user/recovery.md](docs/user/recovery.md) |
| 补丁与验收 / 事故 | [docs/patches/README.md](docs/patches/README.md)、[docs/incidents/README.md](docs/incidents/README.md) |
| 源码树迁移与 Phase 4/5 | [source-tree-migration.md](docs/planning/source-tree-migration.md)、[phase4](docs/planning/phase4-device-validation.md)、[phase5](docs/planning/phase5-retirement-design.md) |
| 脚本 / 工具 / 测试入口 | [scripts/README.md](scripts/README.md)、[tools/README.md](tools/README.md)、[tests/README.md](tests/README.md) |

## 致谢

- 上游仓库：[timhant/innogpu-fh2m-debian-trixie](https://github.com/timhant/innogpu-fh2m-debian-trixie)（本仓库 fork 来源）
- Deepin（V23 驱动载荷、打包结构与社区适配方案）
- Innosilicon（FH2M 硬件、驱动来源与固件；第三方许可按来源包和文件声明）
- Imagination Technologies / PowerVR（DDK V119 谱系与参照）
- [picom](https://github.com/yshui/picom)（上游合成器；本仓库维护 patched 构建）
- Debian 项目、DRM/KMS、Mesa、X.Org、PipeWire 等开源生态
- dotconfig（xdisplay 显示引擎，独立仓库维护）

## 许可证

- MIT 仅覆盖本仓库明确自有的脚本、工具、文档、配置和辅助工作，见 [LICENSE](LICENSE)。
- `drivers/` 中导入的源码按各文件头部和上游许可证处理；黑盒对象、用户态库、DDX、固件和其他载荷是第三方内容。清单中的 `vendor-binary` 是来源分类，不是许可证名称，也不单独授予再分发权。
- 逐类边界、来源和待核实项见 [许可证与再分发边界](docs/project/licensing.md)。
