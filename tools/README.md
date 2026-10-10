# 脚本入口、生命周期与风险

`tools/<name>` 是操作者与 CI 的稳定入口。新增脚本必须在本文件登记所有权、状态改变范围和回退方式。
`tools/internal/` 里的程序不是给人记的命令名；门禁和测试直接调用。

## 构建与打包

> **当前入口**：新架构构建器 `build-innogpu-driver.sh`（迁移源码树 + 经审查的新修复 + manifest
> 黑盒载荷，产出 4.0.x-iN）。以下 `build-deepin-coherent.sh`、`build-patchedNN-*.sh` 与历史安装/卸载入口为
> **legacy（保留）**：永久保留作 p27 oracle、版本护栏、回退包与事故证据；**不作为新工作入口**，
> 不移动不删除（Phase 5 第二步后再评估，见 `docs/design/phase5-retirement-design.md`）。

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
| `build-patched28-suspend-resume.sh` | legacy 实验包装 | 在 p27 补丁集合上增加 patch-024 resume 期间 devfreq 电源状态门禁；只用于复现，`4.0.1-i1` 的 s2idle 可见恢复已失败 |
| `check-deb-dkms-build.sh` | 离线编译检查 | 将指定候选 deb 解包到 `/tmp`，针对指定内核 headers 编译 `innogpu.ko` 并校验 vermagic；不注册或安装 DKMS |
| `check-source-parity.sh` | 只读 parity 检查 | 对照对象 `drivers/` 已不在主线。要比较时先检出 tag `innogpu-4x-frozen`；本脚本不修改旧构建器或设备 |
| `phase4-baseline-capture.sh` | 只读基线采集 | Phase 4 安装前 B1-B12 基线采集（dpkg/lsmod/DKMS/modprobe/initramfs/音频内核可见/Picom/用户态一致性/恢复通道），输出存 `baselines/phase4-baseline-<ts>.log`；真实会话项（/dev/dri、Xorg/GL、音频 sink、显示切换）由用户实机执行 |
| `compare-oracle-candidates.sh` | oracle 对比 | 新架构候选包 vs patched-27：对比 control（除 Version/Description/Installed-Size）、文件清单、载荷哈希、DKMS 源码、黑盒对象、maintainer 脚本（版本归一）、版本排序与 module_symbols（调用 compare-module-symbols.sh）；构建产物（.o.cmd/.o/.ko/modules.order/Module.symvers/.mod）统一按 ARTIFACT_RE 排除；输出机器可读 PASS/FAIL |
| `compare-module-symbols.sh` | 只读符号对比 | 离线构建候选与 patched-27 两包 DKMS 源码（同一内核头），逐 .ko 对比 vermagic/depends/导出符号/导入符号；构建于 `$ROOT/.build/`，不安装不重启；module_symbols=PASS/FAIL/UNCOMPARABLE |
| `build-linux-patched-modules.sh` | Linux 第三方补丁构建 | 从 `debs/` 复用或通过 APT 下载精确源码 `.deb`，临时展开并严格应用 001/002，针对已安装内核只构建 `xhci-pci.ko` 与 `battery.ko`；不安装、不重启 |
| `install-linux-patched-modules.sh` | 受监督模块安装 | 校验构建证据，备份原始模块/initramfs，安装到 `updates/r51/` 并刷新 depmod/initramfs；不自动重启 |
| `rollback-linux-patched-modules.sh` | 模块回退 | 删除 `updates/r51/` 覆盖模块并恢复安装前 initramfs；需要 root，不自动重启 |
| `r51-battery-task3-verify.sh` | R51 安装后验收 | 核对已加载 battery/xHCI Build ID 与 `0x90` quirk，指引一轮拔电/插电并自动保存 sysfs/udev/journal 时间线；不触发 PM |
| `r50-grub-task2-audit.sh` | R50 GRUB 只读排查 | 从普通用户自动 sudo，采集 UEFI 变量、ESP/主 GRUB 配置、菜单与加载链到 R50 证据根；不修改宿主 |
| `r50-storage-task3.sh` | R50 大目录盘点与清理 | `--inventory` 只读分类、`--self-test` 复核 80 个目标/三区独立键/无重叠/重建来源；`--confirm` 强制普通用户在物理 TTY 对 `.runtime-archive/.build/build` 逐区确认并留回执；`--apply RECEIPT` 仅按回执绑定的精确清单先生成内容 SHA 再删除，并核对保留集 |
| `r50-storage-task4.py` | R50 `build/` 迁移 | 普通用户启动；`--confirm` 在物理 TTY 用 sudo 只读生成 45 项 content/metadata SHA 并绑定回执，不移动；`--apply RECEIPT` 由受限 sudo 子模式逐项复核并同盘 rename，可在验证已完成项后续跑，拒绝覆盖、双份、缺失和漂移；`--self-test` 验证 rename 身份与 moved/already-moved 语义 |
| `r50-storage-task5.py` | R50 r5dpm 构建物清理 | 普通用户启动；`--inventory` 用 sudo 只读锁定 119 项 content/metadata SHA 与保留证据；`--confirm` 强制物理 TTY 精确短语回执且不删除；`--apply RECEIPT` 仅在候选、保留集、107/101 内核与 GRUB 无漂移时，先记逐文件 SHA 再删除精确目标 |
| `cleanup-linux-module-worktree.sh` | 源码工作树清理 | 只删除本轮展开源码和构建树，保留 `debs/` 源码包、模块与日志 |
| `prune-kernel-source-cache.sh` | 旧源码包清理 | 默认只列出无对应已安装内核的源码 `.deb`；显式 `--delete` 才删除，当前实现面向 Debian |
| `build-innogpu-driver.sh` | **新架构当前构建器** | 默认 `4.0.2-i3`；R49 候选仅 `5.0.0-i12` + epoch `1790812800`：继承已审 i11，严格应用 `030-035` 私有对象 accessor 修复。编译/包内源共用最终派生树门 `9a8d185f2a65`；i1-i11/未知版本/错 epoch 拒绝，ABI 门保持。i12 A/B 与安装通过，运行验收首轮因外接输入未恢复失败停止；历史成绩不改 |
| `generate-fantgpu-maintainer-scripts.sh` | F 构建器共用生成段 | 只向显式 PACKAGE_ROOT 的 DEBIAN/ 写 postinst/prerm/postrm，生产与回归共用。K 全集检查/失败传播；两调用点共用配置门，仅固定 autoinstall_all_kernels.conf 已审 30 字节 SHA 例外，其它有效配置与目录/文件符号链接拒绝，不 source 配置取值。回退丢弃未装包暂存根，已安装包另行批准恢复 |
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
| `uninstall-innogpu.sh` | 卸载驱动、需重启 | 通用卸载器；版本包装见 `uninstall-patched*.sh`。会安全清理源码 fallback 创建的 `/usr/local/sbin` helper 符号链接（仅当指向本项目 `tools/repair-dri-nodes.sh`，用户/包文件不删）并移除 repair unit；其内置恢复提示仍固定指向 patched-17，当前应以 `docs/user/recovery.md` 的 p27 首选链为准 |
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
- `materialize-d-stage.sh`
- `materialize-o-stage.sh`
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
- `check-fantgpu-runtime-health.sh`
- `check-fantgpu-pm-probe-removed.sh`
- `check-hygon-xhci-resume-fix.sh`

`check-fantgpu-runtime-health.sh` 是 F 真机 R 项前的只读硬门禁：需要操作者提供特权采集的完整
`dmesg` 和计算侧 status 文件，并同时检查 DRM sysfs/devfs card、固件请求失败和 kernel fault。
输出 `PASS` 才能继续 R 项；`FAIL` 或 `UNVERIFIED` 都必须停批，不能由 Driver/Firmware `OK`
单独替代显示设备注册证据。

`check-fantgpu-pm-probe-removed.sh` 是 030-032 诊断代码退出发布候选时的只读门禁：同时扫描
源码、物化 snapshot/manifest、builder/DKMS 树、模块和 deb 解包载荷；任一层残留探针符号、
确认 token 或状态字段即失败关闭。

`check-hygon-xhci-resume-fix.sh` 是受影响 Hygon `1d94:148c` 宿主在内核/系统升级后的只读门禁：
从 sysfs 定位控制器，并从当前 boot 日志核 `XHCI_RESET_ON_RESUME` 的 `0x80` quirk 位。
`FAIL` 或 `UNVERIFIED` 均不得继续 suspend 验收；不加载模块、不触发 PM。

`tests/linux/run-battery-delay-static-tests.sh` 锁定 002 补丁的 SHA、KaiTian/X7h G1e 双 DMI 匹配、
既有 delay callback 和 107 基线声明；它只验证补丁形状，不替代安装后的拔插测试。

`run-capability-survey.sh` 编译并运行 Vulkan/OpenCL/VA-API 最小枚举探针并抓取 sysfs 环境快照，输出保存到 `baselines/capability-survey-<ts>.log`（可用 `--out DIR` 改位置）；只读，不 modeset、不改配置。设备无 DRM render 节点时（如无特权容器）优雅降级并记录失败本身。
其中 `check-docs.sh` 检查根入口、`LICENSES/`、`docs/`、`tools/`、`baselines/`、
`tests/`、`tools/` 下的 Markdown 链接，并对受跟踪文档目录与本机 `collab/` 执行隐私扫描，同时检查稳定入口登记、当前版本、
runtime 统计、manifest 原包 SHA、过期状态断言和 Markdown 表格结构。它还调用
`tools/internal/audit-licenses.py` 校验逐文件许可证 inventory、策略、条款 hash、模块元数据和 manifest
许可证证据语义；机械审计通过不解除 `license_release_gate=BLOCKED`。导入源码内容、内联代码路径、
法律授权与文档语义仍须人工审查。
`materialize-d-stage.sh` 是阶段一 D_stage 物化入口：只读 `third_party/` D 快照，按 4.0.2-i3
配方（显式补丁文件名清单，builder 序 + suspend 组）重放补丁到 `/tmp/r16-d-stage/`，补丁失败或
`.orig/.rej` 残留 fail-closed；只写 `/tmp/r16-d-stage/`，不修改仓库任何路径，不安装不 modeset。
`materialize-o-stage.sh` 是阶段三 O_stage 物化入口：只读 `docs/planning/evidence/o-stage/f0-snapshot.tar.zst`
与 `patches/030-*`，按 18 条 030-NNN 链序（-p1 --fuzz=0，每链点 after_tree_hash 校验，最终树 hash
5f6a5347…）物化 O_stage 源树，并以五件同代事务产物（o-stage-snapshot.tar.zst + 双 sidecar +
5.0.0-i6.meta.json 不可变 provenance；版本族经 OSTAGE_* 注入，缺省 5.0.0-i6 代）写入 `docs/planning/evidence/o-stage/5.0.0-i6/`；i2/i3/i4/i5 五件保持失败档案锚点不覆盖（阶段三证据/产物目录；
O-4 F0 输入只读）；tar 1.35/zstd 1.5.7 精确版本锁（不匹配 exit 7）、输出目录 realpath 边界（越界
exit 78）、排他锁（占用 exit 6）、journal+fsync+恢复、禁止混代；故障注入钩子 `OSTAGE_FAIL_INJECT`
（仅测试）。契约 = `docs/design/o-stage-integration-plan.md` §一；回归测试：
`tests/unit/run-o-stage-materialize-tests.sh`（19 用例：路径越界/symlink 拒绝/SHA 不符/坏输入/
工具版本锁/事务故障注入（commit 与 staged_done）/恢复/幂等/链点 fail-closed/回滚失败
rolling_back 保留现场/人工裁决后恢复/committed 写入失败自洽恢复/未知与损坏 journal
fail-closed/恢复清理失败 fail-closed）+ `tests/unit/run-030-meta-tests.sh`
（18 条 meta 链点校验）+ `tests/unit/run-030-031-pm-marker-tests.sh`（15 项 F PM marker 静态契约）+
`tests/unit/run-030-032-pm-probe-tests.sh`（16 项诊断探针静态契约）+
`tests/unit/run-030-033-shipped-abi-tests.sh`（i3 共享结构 ABI 与独立 devres 状态契约）+
`tests/unit/run-030-034-stop-stage-tests.sh`（17 项检查点顺序/errno、re-arm、DMA variant、ABI 与真实逆序撤销静态契约）+
`tests/unit/run-fantgpu-pm-probe-removal-tests.sh`（12 项发布移除门禁）。
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
| `r51-battery-task2.sh` | R51 受授权现场恢复与拔插补证；从普通用户 shell 启动，特权动作由脚本自动 `sudo` 提示密码；仅操作 `PNP0C0A:00` 的 battery unbind/bind，读取 DSDT 并采集 udev/journal/sysfs；不重启、不触发 PM/watchdog；恢复失败时不进入拔插测试；完整输出、真实 rc、时间戳和 `result.txt` 自动落到 `.build/r51-battery-observation-20261002-01/task2-run-<ts>/`，`latest-run.txt` 指向最近一次执行 |
| `restore-bat0.sh` | 现场 BAT0 恢复；从普通用户 shell 启动并自动 `sudo`；不依赖 AC 是否在线，枚举 `PNP0C0A:*` 后执行 battery unbind/bind，必要时只请求一次 `modprobe battery`；不卸载模块、不重启、不触发 PM/watchdog；完整输出、步骤 rc、时间戳和 `result.txt` 自动落到 `.build/bat0-recovery/run-<ts>/`，`latest-run.txt` 指向最近一次执行 |
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

1. 改名或移动前扫描 `tools/`、`tests/`、配置、服务、桌面源码和文档，并提供兼容过渡。
2. 改变系统状态的入口必须在文件头和用户文档中说明 root、重启、modeset、卸载和回退风险。
3. 测试原始日志进入忽略路径，Git 只保留精简结果。
4. 维护规则、隐私和 release 边界见
   [`docs/project/maintenance-policy.md`](../docs/project/maintenance-policy.md)。

R34离线观测实现：`tools/internal/prepare-r5-observation.py`锁定6.12.101输入并向全新副本增加notifier成对边界/prepare进度；
`tools/internal/r5-observation.py`只生成配置规格或检查普通JSON，不挂载/访问tracefs、不安装或触发PM。
实现与运行前置见[双立项说明](../docs/design/r5-vpu-and-observation-implementation.md)。

R41段一修订：生成的导出器只接受新观测身份`6.12.101-r5obs2`与`nop`事件模式，
无函数后代图；每CPU连续最多128条、每次最多2048条、逐条核5ms预算后轮换，默认1ms再调度。
R44按已批准备选方案改为1920数据页/核，16核共120MiB，`buffer_size_kb`为7650；
reader、页描述符/SLUB桶及最小snapshot等已列入生成的memory-budget.json，仍有目标allocator
及动态元数据未实测，总133MiB门保持UNVERIFIED。拒绝完整或超规格snapshot。128字节载荷门
不证明含ring头的事件计费已≤128字节。旧`plan`保持r5obs1历史口径，不能配置新导出器；
`check`按显式schema区分历史子集与新r5obs2配对。wire-check保持两种精确BEGIN参数。
Windows脚本增加每秒心跳/序号缺口/丢弃/flush状态；Windows执行仍须另验。
`tools/internal/r5-observation-upload.py --unit <本机配置>`只输出独立systemd服务文件，不安装或启用；
`--serve <本机配置>`才启动接收端，配置要求listen/peer/port/session/output五项，现场值只放本机。
服务仅接受指定peer的`/observation`和`X-R5-Session`，按SHA保全原件及中断文件，不覆盖旧attempt；
`/health`不报告PM覆盖。独立服务的退出会话/冷启存活未实测，不能以unit语法通过代替。
四元组事件、全量清点、真实吞吐及独立保全尚未闭合，段二不得启动。

R44的`--export-module`同时生成`semantic-contract.json`和`memory-budget.json`，契约由
生产`tools/internal/r5-observation.py`唯一提供；`check`的新schema为`r5obs2-pairs-v1`，内核身份
6.12.101-r5obs2，规范化记录kind=pm_boundary。对象ID/生命周期、completion代次、
task ID/生命周期、call ID及operation/pm_phase必须齐全同一；phase=entry/exit，exit含ret。
gen等含糊别名不静默忽略，未知字段拒绝；缺关联或冲突保留UNKNOWN/UNPAIRED并返回非零。
完整选择子集才输出SEMANTIC_RECORDS_PAIRED，不证明全PM覆盖、根因或原件真实性。
R45新增`--semantic-kernel --source <锁定R34源> --output <全新副本>`，只生成源码；
R45历史产物配置为`CONFIG_LOCALVERSION="-r5obs2"`且关闭LOCALVERSION_AUTO，不继承签署私钥。
导出器要求本实例`power/r5_pair`和`power/r5_dictionary`都启用、无filter/trigger/PID筛选，
全CPU、`nop`和`mono`，开启前ring为空；仍需独占实例、关闭其它事件，并在未来触发前核收完整字典。
R45历史预分配512对象/1024边；字典名过长、热插拔/移动/绑定/依赖变化、代次/prepare/全事件超限即失效，
不会重用旧ID后继续判完整。只改生成观测源码，不改F驱动语义。
`semantic-wire-check <capture> --session <nonce> --pair-format <保存的r5_pair.format>`
`--dictionary-format <保存的r5_dictionary.format>`校验原始字节、字典和六项身份；
事件ID必须取同boot保存format，不从旧窗口猜值，mono时戳用于跨CPU配对，不以导出顺序推因果。
80B配对/104B字典是编译布局；内核未启动，全量清点/allocator实占/真实吞吐及R40其余覆盖面仍未闭合。
新CLI的成功仅代表所选原始子集配对；不签实验放行，尾部丢失仍INCOMPLETE_WITH_LOSS。

R46历史生成器/导出器身份为`6.12.101-r5obs2-r46`，配置LOCALVERSION须同步`-r5obs2-r46`；
历史R45内核/模块和证据不得机械改版或混装。当前实例须额外启用`power/r5_aux`，
三事件都要求无filter/trigger/PID选择，原nop/mono、1920页/CPU、16CPU上限不变。
新CLI使用`semantic-wire-check`并同时提供`--pair-format`、`--dictionary-format`、`--aux-format`，
各format必须来自同boot保存原件；96B aux关联真实prepare调用/移动计数、async决定及阶段进出。
阶段object_id/completion_generation为0，独立命名空间，不伪装设备字典项；void调用返回0仅表示返回。
PM notifier链整体进出不等于链内每个notifier身份齐全，queued不等于worker已运行；
完整性输出含每CPU末尾计数和原始wire位置，缺END/丢失仍INCOMPLETE_WITH_LOSS。
真实allocator/事件包络/内核热路径成本仍未验，不安装、不启动、不触发PM。


R47当前源码候选身份为`6.12.101-r5obs2-r47e`，LOCALVERSION须同步；旧产物保留。
新增`/dev/r5_meter`只在该观测核启动后存在，0600 + CAP_SYS_ADMIN，独立预分配8192×88B日志，
不借被测ring落计量记录。分配/free、page分配/free、percpu/chunk分别记账；M0–M6与END强制顺序，
关闭未结束会话、溢出和NMI漏记均失效。计量期间全局分配是保守超集，不能据此认定所有归属已闭合。
未来另批授权后才可用`meter-session <全新普通输出文件>`读取；M0在实例创建前，操作人依矩阵
输入M1…M6、END；读取工具不创建实例、不加载模块、不触发PM。普通离线文件用`meter-check`
重放；`--bounds`的L/R/C/B仅核显式来源声明的算术，不把自填上界视为实测证明。
R47原始语义判读须带`--require-r47`和三份format；缺详细字典、回调元数据或worker关联不签完整。
Windows脚本身份不变，上传工具零改动。以上是 R47 当时边界。现行结论见 docs/project/status.md：R5 已通过；U1/U2/R9 保持 unverified；validation-results 已签署且字段不改；tag `fantgpu-5.0.0-i12` 指向 `079b179`；不建 Release。真实实占/吞吐与实验申请门仍未闭合。

R47b发射范围按R40：普通resume保留细粒度，其它阶段保留代次/complete及成对摘要。
callback/worker原96B aux携入口，解析器按DETAIL_COUNTS_R40字典标志还原入口，exit仍原pair。
新旧profile不能混配；不合并未结束调用、不丢弃失败调用。总计39,936硬门不变。
用户后续明确批准在当前物理设备安装/启动，只用于非PM计量；原“独立物理机”不再作为
当前设备事实。安装、首启与M0–M6结果分别记录，安装授权不等于挂起实验触发授权。

R47c修正计量路径：public bulk alloc/free补trace、大分配避免内外重复trace，生产驱动语义不变。
专用trace实例须在启用事件前通过实例`trace_options`设置`norecord-cmd`和`norecord-tgid`并回读；
实例不提供顶层的独立`options/record-cmd`文件。前置检查全局事件关闭且没有其它实例，
并核全局选项前后不变；不得放宽导出器检查或改全局trace选项。
END后输入EOF仍须排空计量日志；r47b失败现场单独保全，不拼接为完整M0–M6。

R47d将准入失败定位编入新身份内核与同核导出器：几何门在导出器，census细项在内核内置
emitter，锁释放后才打印；不改上限或PM生产语义。当时`meter-session`仅接受r47d，旧r47c在打开
设备前拒绝；文件解析仍保留旧身份。离线构建/签署通过不等于运行准入，宿主安装/启动/加载
仍停在用户授权边界，不能把旧r47c原件改成r47d证据。

R47e源码候选修正已定位的对象容量不足：单一契约定义4096对象/8192边，queued-call数组与
对象ID同界；全部五表尺寸及4096B控制预留由真实内核编译期断言核为543744B，仍在原1MiB
字典预算内。新字典标记DETAIL_COUNTS_R40_V2，旧标记仍严格512/1024，不放宽历史验收。
当前`meter-session`仅接受r47e，旧r47d在打开设备前拒绝；文件解析保留所有历史身份。
39936总事件门、133MiB总预算和1920页/核不变；新字典准入上限不是全事件覆盖证明。
初始节点只完成目标对象编译/离线回归；用户允许缓存清理与~/tmp落点后，现已完成新完整
内核/同核i11/导出器的离线构建、签署及身份核验；后经集中授权安装/正常启动，首启通过。
一次非PM计量在M2标记检出M1期间丢失5911条日志即停；导出器/census/M4未执行。
真实完整清点/allocator/吞吐仍未闭合，不因字典扩容或编译通过而允许PM。
用户态读取修订仅合并至多64次原16条read，通过16块有界队列交给单写盘线程；不改变内核
日志容量/ABI或观测模块，写失败/队列满/收尾超时非零。每块至多90112B、队列载荷至多
1441792B，另有读写在途载荷和Python运行时开销；此工具扰动须在实占归属中核算，
不能当作观测器总预算已闭合。接收脚本与上传工具保持不变；现boot不重开计量。
