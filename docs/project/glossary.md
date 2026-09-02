# 术语表

本表只解释本项目反复出现、且会影响维护或验收判断的名称。当前版本角色以
[`status.md`](status.md) 为准，命令和风险以 [`../../scripts/README.md`](../../scripts/README.md) 为准。

| 术语 | 含义 | 本项目中的边界 |
| --- | --- | --- |
| Innogpu / Fantasy II-M / FH2M | 本项目支持的 Innosilicon 图形设备及其驱动、renderer 和 firmware 名称 | 三者描述同一设备栈的不同层，不表示可混用的独立驱动 |
| patched-N | 在原始驱动包上建立的版本化候选 | 版本号同时标识载荷、补丁集和验收证据；不得复用 |
| Deepin 202504 原包 | `20250421190503-debug` 的完整上游 deb | 后续候选唯一的 DKMS 源码、用户态 ABI、固件和 DDX 载荷基线 |
| coherent | 同一 Deepin 发布的 DKMS、DRI、GBM、GLAPI、GLVND、DDX、固件和安装脚本整体部署 | 禁止从历史 patched 包挑选单个 `.so` 或固件拼装 |
| DKMS | Dynamic Kernel Module Support，按当前内核编译并安装内核模块的机制 | 包安装成功不等于内存中的旧模块已被替换，仍须重启验证 |
| DRM / fbdev | 内核的图形设备接口；fbdev 提供 `/dev/fb0` framebuffer | `card0`、`renderD128` 与 `fb0` 是独立验收项 |
| DMA-BUF / PRIME self-import | DRM PRIME 可把 GEM 对象导出为 DMA-BUF fd，并可再导入为 GEM handle；self-import 指同一设备重新导入自己导出的同一对象 | 当前 PASS 仅覆盖同设备 PRIME self-import，不代表 foreign、跨设备、GBM 或 V4L2 导入已验证 |
| DDX | Xorg 的设备驱动模块 | 本项目的 `innogpu_drv.so` 必须与同源用户态一起部署 |
| DRI / GBM / GLAPI / GLVND | Mesa/X11 图形用户态的直接渲染、buffer 管理、GL API 和 vendor 分发组件 | 它们与 DDX、firmware 构成不可拆分的 ABI 载荷 |
| PVR | PowerVR 服务层，负责本设备的固件和图形 services 初始化 | PVR ACTIVE 与 Driver/Firmware OK 是内核运行验收的一部分 |
| GLX / EGL | X11 与无窗口图形环境使用的 OpenGL 上下文接口 | 本机桌面主路径是 GLX；EGL 诊断失败不能单独否定 GLX 结论 |
| RandR | Xorg 的输出、模式和布局扩展 | 通用布局由 dotconfig 的 xdisplay 维护，本仓库仅提供设备钩子 |
| TTY / VT | 文本登录终端和虚拟控制台 | 真实 VT 上的 fbterm 用于验证 fbdev，不能用 Xorg 桌面结果替代 |
| SOURCE_DATE_EPOCH / 可复现构建 | 固定发布时间戳，使同一源码和开关重复构建逐字一致 | 构建器必须在打包前把整树 mtime 归一化到该 epoch（2026-08-20 release 审阅修复）；哈希不一致禁止直接发布 |
| `binary-manifest.json` | 记录黑盒载荷来源、路径、哈希、大小和类型的唯一清单（192 项） | 载荷不入库；`vendor/` 由提取工具按清单从 Deepin 原包幂等重建 |
| `vendor-binary` | manifest 中条目的许可证分类值 | 是**来源分类**，不是 SPDX/许可证名称，也不单独授予再分发权（见 [licensing.md](licensing.md)） |
| dsh / codex | 本项目多 Agent 协作中的监督者 / 实现者 | dsh 审查与批准，codex 实现与汇报；用户对重大事项最终拍板（见 [multiagent-collab.md](multiagent-collab.md)） |
| `collab/` | 多 Agent 轮次的 request/report 本机存档 | 被 Git 忽略，不上传 GitHub、不自动重许可、不进公开制品 |
| `CLEARED` / `BLOCKED` | 某个机械发布门禁通过 / 不可发布 | `project-tools=CLEARED` 只表示候选制品通过机械检查；整仓 `license_release_gate=BLOCKED` 与 `driver-source=BLOCKED` 仍保持，不可互相替代 |
| `4.0.0-i1` | 新架构基线（drivers/ 源码树 + manifest 黑盒载荷） | Phase 4 已验证；`3.3.3.42-patched-27` 为保留的首选回退基线 |
| `4.0.1-i1` | patch-024 suspend/resume 修复的失败候选 | 独立升号，固定 epoch 1788278400；已构建/安装，s2idle 唤醒红屏后回退，deep 未测试，禁止安装 |
| `4.0.1-i2` | 当前运行的 patch-024 + patch-025 候选 | R05 一次 s2idle 可见恢复通过；不复用为严格 A/B，因果仍 UNVERIFIED |
| `4.0.1-i3` / `4.0.1-i4` | R06 严格 A/B：i3 仅 024，i4 为 024+025 | 共用 epoch 1788451200；包级单变量准备通过，四轮 s2idle 待执行 |
