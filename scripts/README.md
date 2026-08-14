# 脚本入口、生命周期与风险

`scripts/<name>` 是兼容接口。安装器、测试、release 和外部文档可能直接调用这些路径，因此不为目录
美观批量改名；新增脚本必须在本文件登记所有权、状态改变范围和回退方式。

## 构建与打包

| 入口 | 生命周期 | 职责 |
| --- | --- | --- |
| `build-deepin-coherent.sh` | 当前公共构建器 | 从完整 Deepin 202504 原包构建 coherent deb，版本、release epoch 和所有功能均由显式参数控制 |
| `build-patched21-deepin-release-candidate.sh` | 当前固定包装 | 以固定 p21 开关构建所有权收敛后的首个 release candidate；只构建，不安装 |
| `build-patched17-deepin-local-display.sh` | 停用护栏 | 明确拒绝把 patched-17 作为后续构建父版本 |
| `build-patched18-deepin-local-display.sh` | 停用护栏 | 明确拒绝重建历史混合载荷 patched-18 |
| `build-patched19-deepin-coherent.sh` | 停用护栏 | 明确拒绝用当前辅助载荷复用 patched-19 版本号 |
| `build-patched20-deepin-diagnostic.sh` | 停用护栏 | 明确拒绝用当前辅助载荷复用已验收 patched-20 版本号 |
| `prepare-deepin-userspace-root.sh` | 当前辅助 | 将 Deepin 原包解包到被忽略的 `third_party/` |
| `build-patched-picom.sh` | 独立组件 | 构建/安装固定基线的 patched Picom，不进入驱动 deb |

## 支持的安装与恢复

| 入口 | 风险 | 说明 |
| --- | --- | --- |
| `install-prereqs-debian.sh` | 修改软件包 | 安装 Debian 构建和运行依赖 |
| `install.sh` | 修改驱动、需重启 | 只调度 patched-8/17；不把 patched-20 诊断候选设为默认 |
| `install-patched17-and-check.sh` | 修改驱动、需重启 | 当前新设备保守入口和 patched-20 回退入口 |
| `install-patched8-and-check.sh` | 修改驱动、需重启 | 更早的历史恢复入口 |
| `uninstall-innogpu.sh` | 卸载驱动、需重启 | 通用卸载器；版本包装见 `uninstall-patched*.sh` |
| `uninstall-patched17.sh` | 卸载驱动、需重启 | 仅允许卸载版本精确匹配 patched-17 的兼容包装 |
| `uninstall-patched8.sh` | 卸载驱动、需重启 | 仅允许卸载版本精确匹配 patched-8 的兼容包装 |
| `disable-incompatible-userspace.sh` | 修改 `/usr` 和 Xorg 配置 | 恢复软件渲染/兼容用户态边界 |
| `restore-tty1-login.sh` | 修改系统服务 | 优先恢复可见 TTY 登录 |
| `prepare-soft-xorg-dwm.sh` | 修改 Xorg/会话 | 准备软件 Xorg；若 dotconfig xdisplay 已存在则接入，否则保留软件路径并警告 |
| `repair-dri-nodes.sh` | 修改 `/dev` 节点 | 根据 sysfs 设备号临时恢复缺失的 DRM/fbdev 节点 |
| `install-dri-node-repair-service.sh` | 安装系统服务 | 固化 DRM/fbdev 节点权限恢复 |
| `install-hygon-hda-audio.sh` | 安装系统/用户服务 | 固化本机 HDA 和 PipeWire 恢复 |

## Picom 接入

| 入口 | 状态改变范围 | 说明 |
| --- | --- | --- |
| `install-picom-prereqs-debian.sh` | 安装软件包 | 安装固定 Picom 基线的 Debian 构建依赖 |
| `install-picom-user.sh` | 修改目标用户配置 | 安装项目 Picom 配置和单一 xprofile 会话入口 |
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
- `check-release-package.sh`
- `check-soft-xorg-dwm.sh`
- `test-current-xorg-hwgl-runtime.sh`
- `test-isolated-deepin-egl-gbm.sh`
- `test-isolated-deepin-hwgl.sh`
- `test-isolated-deepin-xorg-ddx.sh`
- `test-xorg-once.sh`
- `run-local-ddx-vt-test.sh`
- `run-deepin-gbm-egl.sh`
- `run-deepin-surfaceless-egl.sh`
- `verify-install-status.sh`

其中 `check-docs.sh` 检查文档链接、隐私标记、稳定入口登记和固定版本护栏；
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

## 修改规则

1. 改名或移动前扫描 `scripts/`、`tests/`、配置、服务、桌面源码和文档，并提供兼容过渡。
2. 改变系统状态的入口必须在文件头和用户文档中说明 root、重启、modeset、卸载和回退风险。
3. 测试原始日志进入忽略路径，Git 只保留精简结果。
4. 维护规则、隐私和 release 边界见
   [`docs/project/maintenance-policy.md`](../docs/project/maintenance-policy.md)。
