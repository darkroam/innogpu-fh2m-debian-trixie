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
| DDX | Xorg 的设备驱动模块 | 本项目的 `innogpu_drv.so` 必须与同源用户态一起部署 |
| DRI / GBM / GLAPI / GLVND | Mesa/X11 图形用户态的直接渲染、buffer 管理、GL API 和 vendor 分发组件 | 它们与 DDX、firmware 构成不可拆分的 ABI 载荷 |
| PVR | PowerVR 服务层，负责本设备的固件和图形 services 初始化 | PVR ACTIVE 与 Driver/Firmware OK 是内核运行验收的一部分 |
| GLX / EGL | X11 与无窗口图形环境使用的 OpenGL 上下文接口 | 本机桌面主路径是 GLX；EGL 诊断失败不能单独否定 GLX 结论 |
| RandR | Xorg 的输出、模式和布局扩展 | 通用布局由 dotconfig 的 xdisplay 维护，本仓库仅提供设备钩子 |
| TTY / VT | 文本登录终端和虚拟控制台 | 真实 VT 上的 fbterm 用于验证 fbdev，不能用 Xorg 桌面结果替代 |
| SOURCE_DATE_EPOCH / 可复现构建 | 固定发布时间戳，使同一源码和开关重复构建逐字一致 | 构建器必须在打包前把整树 mtime 归一化到该 epoch（2026-08-20 release 审阅修复）；哈希不一致禁止直接发布 |
