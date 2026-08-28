# 脚本入口、生命周期与风险

`scripts/<name>` 是兼容接口。安装器、测试、release 和外部文档可能直接调用这些路径，因此不为目录
美观批量改名；新增脚本必须在本文件登记所有权、状态改变范围和回退方式。

## 构建与打包

> **当前入口**：新架构构建器 `build-innogpu-driver.sh`（迁移源码树 + manifest 黑盒载荷，产出
> 4.0.0-iN）。以下 `build-deepin-coherent.sh`、`build-patchedNN-*.sh` 与历史安装/卸载入口为
> **legacy（保留）**：永久保留作 p27 oracle、版本护栏、回退包与事故证据；**不作为新工作入口**，
> 不移动不删除（Phase 5 第二步后再评估，见 `docs/planning/phase5-retirement-design.md`）。

| 入口 | 生命周期 | 职责 |
| --- | --- | --- |
| `build-deepin-coherent.sh` | legacy 构建器（保留） | 从完整 Deepin 202504 原包构建 patched-N 系 coherent deb；p27 oracle 与 check-docs 版本护栏依赖，禁止删除；版本、release epoch 和所有功能均由显式参数控制 |
| `build-patched21-deepin-release-candidate.sh` | legacy 包装（保留） | 以固定 p21 开关构建所有权收敛后的首个 release candidate；只构建，不安装 |
| `build-patched22-local-lid.sh` | legacy 包装（保留） | 以 patch-009 修正本机内置 eDP connector；脚本只构建，安装和重启由操作者显式执行 |
| `build-patched23-invisible-read-fix.sh` | legacy 包装（保留） | 在 p22 补丁集合上增加 invisible READ mapping 释放不回写修复；历史上只构建不安装，当前仅作 p23 复现/证据入口 |
| `build-patched24-kernel-612101.sh` | legacy 包装（保留） | 在 p23 补丁集合上增加 `6.12.101+` 的 `pci_resize_resource()` 兼容修复；当前仅作 p24 复现/证据入口 |
| `build-patched25-dma-resv-fix.sh` | legacy 包装（保留） | 在 p24 补丁集合上增加 patch-025 dma_resv usage 语义修复；当前仅作 p25 复现/证据入口 |
| `build-patched26-vblank-guard.sh` | legacy 包装（保留） | 在 p25 补丁集合上增加 patch-026 未活动 CRTC vblank 守卫；当前仅作 p26 复现/证据入口 |
| `build-patched27-foreign-dmabuf.sh` | legacy 包装（保留） | 在 p26 补丁集合上增加 patch-027 foreign DMA-BUF 生命周期修复；当前仅作 p27 oracle/复现入口 |
| `check-deb-dkms-build.sh` | 离线编译检查 | 将指定候选 deb 解包到 `/tmp`，针对指定内核 headers 编译 `innogpu.ko` 并校验 vermagic；不注册或安装 DKMS |
| `check-source-parity.sh` | 只读 parity 检查 | 在临时目录重建 p27 生成源码树（third_party + 9 个启用补丁按构建器顺序 + 清理 .orig/.rej），与 `drivers/` 逐文件对比（排除 README/.o_shipped/.o.cmd），输出机器可读 PASS/FAIL；不修改源码树、旧构建器或设备 |
| `phase4-baseline-capture.sh` | 只读基线采集 | Phase 4 安装前 B1-B12 基线采集（dpkg/lsmod/DKMS/modprobe/initramfs/音频内核可见/Picom/用户态一致性/恢复通道），输出存 `baselines/phase4-baseline-<ts>.log`；真实会话项（/dev/dri、Xorg/GL、音频 sink、显示切换）由用户实机执行 |
| `extract-vendor-binaries.sh` | 幂等提取工具 | 按 `binary-manifest.json` 从 pinned Deepin deb 提取黑盒载荷到 `vendor/`；校验 deb SHA、目标存在且哈希一致则跳过、缺失/不符安全重建、拒绝路径穿越/未知 kind、临时目录 + 原子 rename；`--check-only` 只读；输出机器可读 PASS/FAIL；路径可用 `MANIFEST_PATH`/`VENDOR_ROOT` 覆盖（隔离测试用） |
| `generate-binary-manifest.py`（tools/） | 清单生成 | 从 Deepin deb 确定性生成 `binary-manifest.json`（校验 deb SHA、覆盖全部黑盒文件与符号链接、kind/role/license 分类） |
| `compare-oracle-candidates.sh` | oracle 对比 | 新架构候选包 vs patched-27：对比 control（除 Version/Description/Installed-Size）、文件清单、载荷哈希、DKMS 源码、黑盒对象、maintainer 脚本（版本归一）、版本排序与 module_symbols（调用 compare-module-symbols.sh）；构建产物（.o.cmd/.o/.ko/modules.order/Module.symvers/.mod）统一按 ARTIFACT_RE 排除；输出机器可读 PASS/FAIL |
| `compare-module-symbols.sh` | 只读符号对比 | 离线构建候选与 patched-27 两包 DKMS 源码（同一内核头），逐 .ko 对比 vermagic/depends/导出符号/导入符号；构建于 `$ROOT/.build/`，不安装不重启；module_symbols=PASS/FAIL/UNCOMPARABLE |
| `build-innogpu-driver.sh` | **新架构当前构建器** | 装配 drivers/ + vendor 黑盒 + 确定性变换，离线编译 DKMS，产出完整 coherent 包（4.0.0-iN）；版本排序 > patched-27 校验；`SOURCE_DATE_EPOCH` 必填（缺失即失败）；`.o.cmd` 守卫保证构建产物不入包；不安装不重启 |
| `build-patched17-deepin-local-display.sh` | legacy 护栏（保留） | 明确拒绝把 patched-17 作为后续构建父版本 |
| `build-patched18-deepin-local-display.sh` | legacy 护栏（保留） | 明确拒绝重建历史混合载荷 patched-18 |
| `build-patched19-deepin-coherent.sh` | legacy 护栏（保留） | 明确拒绝用当前辅助载荷复用 patched-19 版本号 |
| `build-patched20-deepin-diagnostic.sh` | legacy 护栏（保留） | 明确拒绝用当前辅助载荷复用已验收 patched-20 版本号 |
| `prepare-deepin-userspace-root.sh` | 当前辅助 | 将 Deepin 原包解包到被忽略的 `third_party/` |
| `build-patched-picom.sh` | 独立组件 | 构建/安装固定基线的 patched Picom，不进入驱动 deb |
| `build-patched-fbterm.sh` | 独立组件 | 从 Debian fbterm 1.7-5 构建可配置 redraw 的用户本地版本，不进入驱动 deb |

## 支持的安装与恢复

| 入口 | 风险 | 说明 |
| --- | --- | --- |
| `install-prereqs-debian.sh` | 修改软件包 | 安装 Debian 基础构建和运行依赖；当前未显式安装新构建器直接使用的 `python3`，最小系统须按 `docs/project/dependencies.md` 补充核对 |
| `install.sh` | 修改驱动、需重启 | 只调度 patched-8/17；不把 patched-20 诊断候选设为默认 |
| `install-patched17-and-check.sh` | 修改驱动、需重启 | legacy 深层回退入口；新设备默认入口是 4.0.0-i1 |
| `install-patched8-and-check.sh` | 修改驱动、需重启 | 更早的历史恢复入口 |
| `uninstall-innogpu.sh` | 卸载驱动、需重启 | 通用卸载器；版本包装见 `uninstall-patched*.sh`。会安全清理源码 fallback 创建的 `/usr/local/sbin` helper 符号链接（仅当指向本项目 `scripts/repair-dri-nodes.sh`，用户/包文件不删）并移除 repair unit；其内置恢复提示仍固定指向 patched-17，当前应以 `docs/user/recovery.md` 的 p27 首选链为准 |
| `uninstall-patched17.sh` | 卸载驱动、需重启 | 仅允许卸载版本精确匹配 patched-17 的兼容包装 |
| `uninstall-patched8.sh` | 卸载驱动、需重启 | 仅允许卸载版本精确匹配 patched-8 的兼容包装 |
| `disable-incompatible-userspace.sh` | 修改 `/usr` 和 Xorg 配置 | 恢复软件渲染/兼容用户态边界 |
| `restore-tty1-login.sh` | 修改系统服务 | 优先恢复可见 TTY 登录 |
| `prepare-soft-xorg-dwm.sh` | 修改 Xorg/会话 | 准备软件 Xorg；若 dotconfig xdisplay 已存在则接入，否则保留软件路径并警告 |
| `repair-dri-nodes.sh` | 修改 `/dev` 节点 | 根据 sysfs 设备号临时恢复缺失的 DRM/fbdev 节点 |
| `install-dri-node-repair-service.sh` | 安装系统服务 | 固化 DRM/fbdev 节点权限恢复；helper 仅从受信来源解析（包 `/usr/sbin`→源码 fallback `/usr/local/sbin`，任意 PATH 命中被拒绝），unit `ExecStart` 始终指向实际安装的 helper；`/usr/bin` 为**可选**便利链接（创建失败仅告警不阻断）；`enable/start` 失败传播并统一事务回滚（含恢复既有 unit 字节/权限与 systemd enable 状态）。测试钩子 `INNOGPU_DRI_TEST_ROOT`（默认关闭）供 fixture 无 root 验证 |
| `install-hygon-hda-audio.sh` | 安装系统/用户服务 | 固化本机 HDA 和 PipeWire 恢复；创建系统/用户 unit、helper、modules-load 与 ALSA 配置并可能改写 profile/PipeWire 旧配置；仅部分文件有条件性备份，当前无 systemd-analyze 门禁、对称自动卸载器或 fixture，人工回退边界见 `docs/project/audio-management.md` |

## Picom 接入

| 入口 | 状态改变范围 | 说明 |
| --- | --- | --- |
| `install-picom-prereqs-debian.sh` | 安装软件包 | 安装固定 Picom 基线的 Debian 构建依赖 |
| `install-picom-user.sh` | 修改目标用户配置 | 安装项目 Picom 配置（模板 `components/picom/picom.conf`）和单一 xprofile 会话入口 |
| `picom-session.sh` | 启动用户进程 | 优先启动 patched Picom，缺失时回退 xcompmgr，并避免重复实例 |

## 显示接入

xdisplay 引擎不属于本仓库，源码和测试以 dotconfig 为准。本项目仅保留：

| 入口 | 职责 |
| --- | --- |
| `restore-dp1-mode-x11.sh` | 本设备固定 modeline 恢复钩子 |
| `xdisplay-session.sh` | 注入本设备候选输出和恢复命令，启动已有 xdisplay |
| `install-xdisplay-user.sh` | 安装上述接入，不复制或覆盖 xdisplay/displayselect/共享库 |

详细契约见 [`docs/project/display-management.md`](../docs/project/display-management.md)。

## 只读与临时验证

以下入口默认只读，或只创建 `/tmp`/`baselines/` 下的测试环境；带 VT/Xorg 的脚本仍可能短暂切换
控制台，运行前必须阅读输出中的恢复命令：

- `check-deepin-userspace-coherence.sh`
- `check-docs.sh`
- `check-desktop-hwgl.sh`
- `check-innogpu-progress.sh`
- `check-patched17-baseline.sh`
- `check-post-reboot-hwgl.sh`
- `check-soft-xorg-dwm.sh`
- `check-release-package.sh`
- `test-current-xorg-hwgl-runtime.sh`
- `test-isolated-deepin-egl-gbm.sh`
- `test-isolated-deepin-hwgl.sh`
- `test-isolated-deepin-xorg-ddx.sh`
- `test-xorg-once.sh`
- `run-local-ddx-vt-test.sh`
- `run-deepin-gbm-egl.sh`
- `run-deepin-surfaceless-egl.sh`
- `verify-install-status.sh`

`run-capability-survey.sh` 编译并运行 Vulkan/OpenCL/VA-API 最小枚举探针并抓取 sysfs 环境快照，输出保存到 `baselines/capability-survey-<ts>.log`（可用 `--out DIR` 改位置）；只读，不 modeset、不改配置。设备无 DRM render 节点时（如无特权容器）优雅降级并记录失败本身。
其中 `check-docs.sh` 检查根入口、`LICENSES/`、`drivers/`、`docs/`、`scripts/`、`baselines/`、
`tests/`、`tools/` 下的 Markdown 链接，以及选定项目目录的隐私标记、稳定入口登记、当前版本、
runtime 统计、manifest 原包 SHA、过期状态断言和 Markdown 表格结构。它还调用
`tools/audit-licenses.py` 校验逐文件许可证 inventory、策略、条款 hash、模块元数据和 manifest
许可证证据语义；机械审计通过不解除 `license_release_gate=BLOCKED`。导入源码内容、内联代码路径、
法律授权与文档语义仍须人工审查。
`check-release-package.sh` 只解包读取指定 deb，核对版本、关键载荷、禁止文件和设备接入脚本，
不会安装包。发布包边界的可重复 fixture 见 `tests/package/run-boundary-tests.sh`。
`verify-install-status.sh --require-reboot VERSION` 用于运行验收：除常规状态外，它要求包元数据早于当前
启动、驱动已加载、Driver/Firmware 为 OK 且 DRM/fbdev 节点存在；仅查询安装状态时不加该选项。

## 实验和历史入口

以下脚本会改动活动驱动、用户态或 Xorg，不能进入默认安装流程，也不随 coherent release deb 发布：

| 入口 | 状态与风险 |
| --- | --- |
| `install-kylin-userspace.sh` | 历史 Kylin/UOS 用户态实验，存在 Xorg ABI 24/25 混配风险 |
| `install-experimental-hwgl.sh` | 历史完整 vendor GBM/EGL/GLX 实验，已知错误组合可令 Xorg 崩溃 |
| `patch-skip-first-gpupll.sh` | 直接修改预编译对象或已安装模块；只用于受控构建/恢复 |
| `try-hotload-patched17.sh` | 尝试热替换内核模块，图形会话繁忙时必须停止 |
| `start-soft-xorg-dwm-from-ssh.sh` | 从 SSH 启动临时图形链路，必须保留 TTY 恢复手段 |
| `display-recover-and-diagnose.sh` | 故障恢复编排，会修改显示/Xorg 状态 |
| `install-deepin-desktop-hwgl-trial.sh` | 仅在本地 DDX 门槛通过后启用硬件 GL 试验 |
| `mark-patched17-soft-baseline.sh` | 只适用于 patched-17 历史软渲染基线 |

这些入口保留用于追溯或受控排障，但不得被描述为当前推荐安装方式。

## 包载荷规则

coherent 驱动 deb 只携带运行和恢复所需的 Innogpu 辅助脚本。历史 Kylin 用户态安装器、实验 HWGL
安装器和直接二进制热补丁不得暴露为系统命令。仓库保留它们不等于 release 支持它们。

当前 `4.0.0-i1` 把 12 个项目 helper 实体安装到 `/usr/share/innogpu-fh2m-trixie/`，并为其中 10 个
提供 `/usr/bin/innogpu-*` 与 `/usr/sbin/innogpu-*` 双链接。manifest 还原样导入 vendor 的
`/lib/systemd/system/sw-inno-gl.service` 与 `/usr/sbin/sw-inno-gl`；maintainer scripts 不会启用或
启动该单元。`check-release-package.sh` 当前只强制 3 个显示接入 helper 与若干关键载荷，尚未验证
vendor unit/helper 和全部命令链接，不能把 `PASS_RELEASE_PACKAGE_BOUNDARIES` 扩写为这些路径均已受门禁。

## 修改规则

1. 改名或移动前扫描 `scripts/`、`tests/`、配置、服务、桌面源码和文档，并提供兼容过渡。
2. 改变系统状态的入口必须在文件头和用户文档中说明 root、重启、modeset、卸载和回退风险。
3. 测试原始日志进入忽略路径，Git 只保留精简结果。
4. 维护规则、隐私和 release 边界见
   [`docs/project/maintenance-policy.md`](../docs/project/maintenance-policy.md)。
