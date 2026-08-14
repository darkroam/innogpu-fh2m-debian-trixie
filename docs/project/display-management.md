# 显示管理

## 当前状态

仓库已吸纳 dotfiles 提交 `5628c6e` 的 X11 显示管理实现。`scripts/xdisplay.sh` 与来源文件保持
完全一致，SHA-256 为 `427d56f78ff11482c59c9e4b95f9fc75a1890ca83b83d81956410d17e6690251`；
`displayselect` 在来源基础上仅将个人桌面后处理改为可选。旧的
`scripts/xdisplay.sh.with-innogpu-restore` 已删除。

## 当前实现

来源 watcher 每轮只解析一份 RandR 快照，记录：

- connection、primary、geometry 和正负坐标；
- current、preferred、target 模式及刷新率；
- 模式数量和能力签名；
- active、stale、pending 状态；
- lid 状态、物理拓扑签名和基础 health。

它显式关闭 `disconnected + geometry` 的 stale 输出，并在没有 preferred 时选择 RandR 模式表首项，
不再让 `--auto` 猜测。连接或模式能力变化后进入约 5 秒 settling 窗口；相同失败最多连续写入三次，
随后只在低频主动探测时恢复尝试。

## 运行关系

```text
innogpu + Xorg
  -> RandR outputs/modes/geometry
  -> xdisplay.sh --watch
       -> /proc/acpi/button/lid/*/state
       -> /sys/class/drm/card*-*/status
       -> xrandr --current / --query
       -> 串行应用并验证布局

displayselect -> 同一 apply lock -> xrandr
```

watcher 应由 X11 会话启动并继承 `DISPLAY`、`XAUTHORITY` 和用户环境。不要另建 udev 直接调用
`xrandr` 的链路，也不要在没有完整图形会话环境的 systemd 服务中启动第二个 watcher。

用户文件由 `install-xdisplay-user.sh` 安装，`prepare-soft-xorg-dwm.sh` 只负责调用它并继续完成
系统级软渲染准备。安装器只管理 `~/.local/bin` 中的显示命令、
`~/.config/x11/innogpu-display-session.sh` 和 `xprofile` 中带边界标记的启动块；检测到已有
`xdisplay.sh --watch` 时不得追加第二个 watcher。

`displayselect` 的 RandR 布局操作不依赖个人 dotfiles。壁纸刷新、键盘重映射和通知守护进程重启
属于可选桌面后处理；`setbg`、`remaps`、`dunst` 或 `notify-send` 不存在或失败时必须跳过，
不能把已成功的显示切换报告为失败。

## 本设备边界

- DRM `DP-1`、`HDMI-A-1`、`HDMI-A-2` 当前映射为 RandR `eDP-1`、`HDMI-1`、`HDMI-2`。
- 通用代码不硬编码外屏；本设备仅补充内屏候选 `eDP-1 DP-1`。
- `restore-dp1-mode-x11.sh` 带固定 modeline，只是 Innogpu 设备恢复钩子，不是通用布局策略。
- logind 决定合盖是否挂起；watcher 只在 X11 会话仍运行时处理布局。
- framebuffer 应收敛到有效输出包围盒。保留旧 framebuffer 只能用于受控诊断，不能成为默认策略。

## fbterm 与 framebuffer 映射诊断

在 `3.3.3.42-patched-17` 的当前运行环境中，`fbterm 1.7` 在真实 VT 上报告段错误。独立的
framebuffer 探针已复现其关键前置条件：

1. `/dev/fb0` 可以以读写方式打开；
2. `FBIOGET_FSCREENINFO` 和 `FBIOGET_VSCREENINFO` 均成功返回；
3. 对 `fix.smem_len` 执行 `mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)` 返回
   `-1` 和 `ENODEV`。

`fbterm 1.7` 没有在所有绘制路径上检查 `mmap()` 的 `MAP_FAILED` 返回值，映射失败后继续使用映射
指针即可解释后续 `SIGSEGV`。这说明当前首先应修复或验证 InnoGPU fbdev/GEM 的用户态映射路径；
只给 fbterm 增加错误检查只能把段错误变成清晰的启动失败，不能恢复 framebuffer 终端功能。

该故障与 patched-17 使用的用户态库变化有关联可能，但 `fbterm` 的 `NEEDED` 依赖不包含
`innogpu_drv.so`、`innogpu_dri.so` 或其他 InnoGPU `.so`；Xorg 通过 DRM/RandR 间接使用这些库，
不能据此认定它们是 fbterm 的直接崩溃原因。当前 Xorg 进程也未打开 `/dev/fb0`，所以不能用 Xorg
是否持有 framebuffer 设备替代 VT 复现。

### 复现与对照步骤

探针应只读查询 framebuffer 参数并检查映射结果，不执行 modeset：

```sh
cc -Wall -Wextra -O2 /tmp/fbprobe.c -o /tmp/fbprobe
strace -o /tmp/fbprobe.strace /tmp/fbprobe
```

patched-17 以前的源码与包不再作为本次修复的实现参考；patched-8 只保留为历史回滚物。禁止
把任何旧 `.so` 与新内核/固件混用。后续修复必须以 Deepin 202504 原包的完整内核与用户态载荷
重建，patched-17 仅保留为当前可用回退包，patched-18 仅保留为 fbdev 修复证据；二者都不是后续
版本的源码或打包父版本。在同一设备上重新运行探针并确认 `mmap()` 不再返回 `ENODEV`。真实 VT 中的
`fbterm` strace 仍需由本机 VT 会话直接采集；从 SSH、X11 终端或普通管道运行只会触发
`stdin isn't a tty`，不能证明 VT 绘制阶段行为。

### 当前判断与后续边界

当前最小故障链为：`patched-17` 内核/预编译内存实现 -> InnoGPU fbdev 对 `/dev/fb0` 的 mmap
返回 `ENODEV` -> fbterm 未检查失败指针 -> 绘制阶段段错误。驱动修复需要先确认
`innodpu_drm_fb.c` 的 fbdev 注册、`cpu_paddr`/对象大小和 GEM mmap 语义；不能直接套用普通的
`remap_pfn_range`。在此之前，`.config/shell/zprofile` 保持禁用 fbterm，避免把已知不稳定的
framebuffer 路径作为默认终端。

历史版本对照曾用于定位 `.fb_mmap` 缺失，但不再作为后续实现依据。当前有效证据是：Deepin 202504
源码中的 `s_inno_fbdev_ops` 没有显式 `.fb_mmap`；在该源码上增加 `fb_io_mmap` 后，patched-18 的
实机 `/dev/fb0 mmap()` 已成功。后续只在 Deepin 202504 基线上保留并验证该最小修复。

### patched-18 修复候选

`patched-18` 的最小源代码修复位于 `patches/007-fbdev-io-mmap.patch`：为 `s_inno_fbdev_ops` 显式
设置 `.fb_mmap = fb_io_mmap`，让 `/dev/fb0` 使用 Linux 针对 I/O framebuffer 的映射实现，而
不是通用的线性内存路径。原 `build-patched10-deepin.sh` 已删除，patched-17/18 构建入口也已改为
明确失败，避免继续生成带历史混合载荷的同版本包。后续只有
`scripts/build-deepin-coherent.sh` 在 `APPLY_FBDEV_IO_MMAP=1` 时应用该补丁；
`scripts/build-patched19-deepin-coherent.sh` 是当前候选入口。

patched-18 测试包已在仓库内完成重建，并检查包版本为 `3.3.3.42-patched-18`、包内源码包含
`fb_io_mmap`；当前构建产物的 SHA-256 为
`caccac417171109837f453ce3d6d4adc7a0cb35207b1d3857f0425b5c82ac612`。当前主机已安装该包并完成
`6.12.96+deb13-amd64` 的 DKMS 构建、启动配置和首次重启。实机 framebuffer 探针得到：

```text
id=innogpudrmfb smem_start=0x7fce3500000 smem_len=16384000 mode=1920x1080 bpp=32
mmap=OK length=16384000
```

这证明 patched-18 的 fbdev mmap 目标已收敛，但不能替代 Xorg 和 fbterm 验收；在桌面恢复且真实 VT
中的 fbterm 验证完成前，patched-18 仍是候选版本。

### patched-18 首次启动的用户态阻塞

首次重启后，InnoGPU DRM、PVR、`card0`、`renderD128` 和 `fb0` 均完成初始化，内核日志未出现
InnoGPU Oops。日志中的 Call Trace 来自无关的 `think_lmi` 固件接口警告。Xorg 则在
`innogpu_gbm.so -> libgbm.so.1 -> gbm_create_device() -> igpu_glamor_egl_init()` 中段错误，标准错误在
崩溃前明确报告：

```text
MESA-LOADER: failed to open innogpu: innogpu_dri.so: undefined symbol: _glapi_tls_Dispatch
```

包的 `postinst` 在安装时把活动的 `innogpu_dri.so` 移为 `.bak`。原位恢复该文件后，隔离的
Xorg `:9/vt8` 测试仍以退出码 134 复现相同错误。符号检查确认这是用户态 ABI 混配：恢复的
2024-11 DRI 文件 SHA-256 为
`e193d91deaf48ae4268187b216fadd4f1cf157f4b5ecbccf50de1300bb1eaa7d`，依赖无 `_priv` 后缀的
`_glapi_*` 符号；当前 Deepin 202504 `libglapi_inno.so.0` 只导出对应的 `_priv` 符号。仓库中的
Deepin 202504 DRI 文件 SHA-256 为
`f36cb1e48bbcd7ba8f4d41c863433816217eed3178078b8c228b471aa0f66cf6`，与当前 libglapi 的符号契约
一致，`ldd -r` 未报告未解析符号。

包内 patched-17 与 patched-18 DKMS 源码的非二进制差异只有 `linux/fb.h` 引入和
`.fb_mmap = fb_io_mmap`。因此当前 Xorg 失败不能归因于新增 DRM ioctl 或其他未记录的内核改动，
但暴露了安装流程缺陷：在已启用硬件 GL 的系统上，驱动升级不能无条件禁用单个 DRI 文件，也不能
恢复与现有 vendor libglapi 不同代的 `.bak`。后续必须先用完整、同源的用户态文件集合做隔离验证，
再恢复桌面；同时修正后续包的 `postinst`，避免再次破坏已经验证的硬件 GL 状态。

进一步审计确认，patched-18 构建器虽然替换了 Deepin 202504 DKMS 源码，却仍以 patched-8 deb
作为包载荷和 maintainer scripts 基础。这种“新内核源码 + 历史包用户态”的拼接没有版本延续性，
也是活动 DRI 留在 2024-11、而 GBM/GLAPI/DDX 已来自 Deepin 202504 的直接原因。修复方案不是继续
给 patched-18 补单个 `.so`，而是让下一候选包从 Deepin 202504 原包整体重建，并在安装前后检查
关键用户态文件的来源和未解析符号。

### patched-19 完整 Deepin 基线候选

`scripts/build-patched19-deepin-coherent.sh` 直接解包 Deepin
`innogpu-fh2m 20250421190503-debug` 原包，不接受 `BASE_DEB` 或任何 patched 包作为输入。它整体保留
原包中的 DRI、GBM、GLAPI、GLVND、DDX、固件和链接关系，仅在原包 DKMS 源码上应用 Debian 6.12、
本地 connector、DP fbcon 与 `fb_io_mmap` 补丁，并替换为本项目审查过的 Debian maintainer scripts。
新 `postinst` 不移动、备份或恢复任何单独的 vendor `.so`。

离线构建已通过以下门槛：包版本为 `3.3.3.42-patched-19`；包内 `innogpu_dri.so`、GBM 模块、vendor
`libgbm`、`libglapi_inno` 和 `innogpu_drv.so` 的 SHA-256 与 Deepin 原包逐字一致；打包后的 DRI 在
同源库路径下执行 `ldd -r` 没有未解析符号；DKMS 源码包含 `.fb_mmap = fb_io_mmap`。当前产物
SHA-256 为 `a9cd51dd53a817839b66493cadcaa95b86ed21ccf6c92fd45196b395160649a6`。

上述是离线构建结论，不是运行成功结论。安装前先用 Deepin 202504 DRI 对当前隔离 Xorg 进行验证；
安装后依次验证 DKMS、DRM/fbdev、隔离 Xorg/GLX、正常桌面和真实 VT fbterm。任一步失败都保留
patched-17 回退包，不通过替换单个 `.so` 尝试收敛。

安装前的非特权 GBM 对照尚未通过：`check-deepin-userspace-coherence.sh` 成功，但在当前 patched-18
内核和完整 Deepin 202504 隔离目录下，`eglinfo` 仍段错误；最小 GBM/GLES2 探针在 loader 报告缺少
`zink_innogpu_dri.so` 后也以 139 退出。该结果不修改系统且不能判定 patched-19 安装结果，但证明
静态哈希和 `ldd -r` 不是充分条件。必须先完成临时 Deepin DRI 的隔离 Xorg/GLX 测试；未通过前
不得安装 patched-19。

### Deepin DRI 隔离测试的 PVR services 阻塞

在当前 patched-18 内核上给临时 Xorg `:9/vt8` 显式指定 Deepin 202504 DRI 后，原先的
`_glapi_tls_Dispatch` 未解析符号不再出现，证明同源 DRI/GLAPI 已越过旧的 ABI 混配故障。但是 Xorg
仍在 `innogpu_gbm.so -> libgbm.so.1 -> gbm_create_device() -> igpu_glamor_egl_init()` 路径中崩溃，
因此 patched-19 仍未达到安装门槛。

对最小 GBM/GLES2 探针执行 `strace` 后得到以下顺序：

1. loader 成功打开 Deepin `innogpu_dri.so`、`libglapi_inno.so`、`libinno_dri_support.so` 和
   `libsrv_um_inno.so`；
2. 用户态成功打开 `/dev/dri/renderD128`；
3. 用户态向 render 节点发出私有命令 5，即源码中定义的 `DRM_PVR_SRVKM_INIT`；
4. 内核的 `drm_pvr_srvkm_init()` 在 `PVR_SRVKM_SERVICES_INIT` 分支调用
   `PVRSRVDeviceServicesOpen(priv->dev_node, psDRMFile)`，并向用户态返回 `ENODEV`；
5. loader 随后才尝试 zink 回退，因不存在 `zink_innogpu_dri.so` 而失败，vendor GBM 最终在失败清理
   路径中发生空指针段错误。

因此当前第一根因边界是 PVR services 对 render 节点的连接初始化返回 `ENODEV`；缺少
`zink_innogpu_dri.so` 和之后的 GBM 段错误均是下游回退/错误处理问题，不能通过补一个 zink 文件或
修改 Xorg、Picom、显示 watcher 配置来修复。Deepin 源码将 `PVRSRV_DEVICE_INIT_MODE` 配置为
`PVRSRV_LINUX_DEV_INIT_ON_CONNECT`，所以打开 DRM 文件本身不完成 services 初始化，首个用户态
`DRM_PVR_SRVKM_INIT` 才是关键边界。

本次启动内核日志同时保留两组证据。一方面，PVR 已读取 BVNC `35.4.1632.23`、注册 RGX 设备、绑定
`inno-gpu` 并完成 `innogpu 2.19.88877759` DRM 注册；另一方面，日志报告
`innogpu/hwinfo_g0m.bin` 加载失败 `(-2)` 以及 `dev_rsrc is NULL or hwinfo register fail`。目前尚无
代码级证据证明 hwinfo 缺失就是 `PVRSRVDeviceServicesOpen()` 返回 `ENODEV` 的直接条件，不能在没有
诊断日志的情况下把相关性写成因果结论。

已确认的配置关联是：当前 `/etc/modprobe.d/innogpu.conf` 设置 `options innogpu firmware_en=1`，而
Deepin 202504 原包仅提供 `fh2m.fw`/`fh2c.fw`，不包含 `innogpu/hwinfo_g0m.bin`。该参数会启用
`hwinfo_register()` 的固件分析路径；固件请求失败后，驱动仍继续注册 DRM/PVR，但 hwinfo 查询保持
失败状态。因此 `firmware_en=1` 与本机的 hwinfo 缺失是下一轮复现的首要变量，但在没有确认
`PVRSRV_DEVICE_STATE_BAD` 的现场值前，不能把它单独认定为 services `ENODEV` 的根因。

反汇编 Deepin shipped `innosrvkm.o_shipped` 得到更窄的判断：`PVRSRVDeviceServicesOpen()` 先检查
全局 PVRSRV 数据是否为空，随后检查 `psDeviceNode->eDevState == PVRSRV_DEVICE_STATE_BAD`；任一条件
成立就返回 `PVRSRV_ERROR_NOT_INITIALISED`（内核 ioctl 映射为 `-ENODEV`）。`PVRSRV_DEVICE_STATE_BAD`
的枚举值为 7。`PVRSRVCommonDeviceInitialise()` 的 `RGXInit()` 错误路径和
`PVRSRVDeviceFinalise()` 错误路径都明确调用 `PVRSRVDeviceSetState(..., 7)`；`MMU_InitDevice()` 错误
路径在该函数中记录错误后直接返回，未看到同一函数内的 `BAD` 状态设置，是否由 MMU 内部或后续状态机
转换仍需运行时证据。因此当前必须区分“hwinfo 缺失导致上游设备初始化失败”和“其他 RGX/MMU/固件
错误”，并直接记录 device state 与各初始化返回码，不能仅凭 `ENODEV` 反推具体失败阶段。

下一步必须继续使用 Deepin 202504 完整基线：定位 services open 的实际实现和所有 `-ENODEV`
分支；若该实现位于预编译 PVR 对象而不可直接审计，则在 `drm_pvr_srvkm_init()` 添加最小诊断，记录
`init_module`、`priv`、`priv->dev_node`、`priv->dev_node->eDevState` 和 services open 返回值；同时
在 `PVRSRVCommonDeviceInitialise()` 的错误路径记录 `MMU_InitDevice()`、`RGXInit()` 与
`PVRSRVDeviceFinalise()` 的返回码。新诊断候选应使用 patched-20 之类的新版本号从 Deepin 原包完整构建，禁止改写
patched-19 的历史产物，也禁止从 patched-8、patched-17 或 patched-18 局部复制 `.so` 或维护脚本。
在隔离 Xorg 门槛通过前不得安装 patched-19。

#### 第二次隔离 Xorg 复核

2026-08-14 12:14:33 使用当前 `/etc/X11/xorg.conf` 再次运行
`scripts/test-current-xorg-hwgl-runtime.sh`，结果与首次复现一致：Xorg `:9/vt8` 以退出码 134
中止，`xdpyinfo` 和 `glxinfo` 均未连接成功。日志仍在
`innogpu_gbm.so -> libgbm.so.1 -> gbm_create_device() -> igpu_glamor_egl_init()` 处段错误，
没有重新出现 `_glapi_tls_Dispatch` 未解析符号。该复核只读取并启动临时 Xorg，不改变持久配置，
因此确认当前阻塞已经稳定地位于 PVR services/GBM 初始化阶段，而不是一次性的 loader ABI 偶发错误。

对应证据保存在 `baselines/latest-current-xorg-hwgl-test/`，包括 `Xorg.log`、`xorg.stderr`、
`summary.txt` 和 `result.txt`；本次内核日志副本为用户主目录的 `p18-kernel.log`，Xorg 测试输出为
`p18-xorg-test.txt`。

当前通过 SSH 排障，图形会话不可用，而且提权密码提示可能转发到 SSH 中不可见的 `dmenupass`。
需要 root 的安装、模块替换或重启命令必须由本机用户明确执行；排障脚本不得热卸载当前驱动或破坏
现有可启动回退路径。每轮测试应保留 Xorg 摘要、loader/strace 顺序和对应启动内核日志，使以后出现
相同故障时能先区分 ABI 混配、services 初始化失败与下游 zink 回退失败。

该问题不改变 X11 显示 watcher 的职责，也不应通过修改布局、Picom 或字体配置规避。相关对照
结果应补充到本节，并在驱动修复后重新运行用户态 framebuffer 和 X11 验证。

#### 内核日志确证：shader 固件载荷缺失

用户提供的 `p18-after-xorg-kernel.log` 已确认完整失败链（2026-08-14 12:10:40）：

1. `innogpu/fh2m.fw` 主固件成功加载；
2. `innogpu/fh2m.sh`、`innogpu/innogpu.sh.35.4.1632.23` 和
   `innogpu.sh.35.4p.1632.23` 均请求失败，返回 `-2`；
3. `PVRSRVTQLoadShaders` 报 `PVRSRV_ERROR_NOT_FOUND`，随后
   `RGXInitDevPart2`、`RGXInit` 和 `PVRSRVCommonDeviceInitialise` 依次失败；
4. `PVRSRVDeviceFinalise` 返回 `PVRSRV_ERROR_NOT_INITIALISED`，设备进入 BAD 状态；
5. 后续 12:14:33、12:17:03、12:17:25 和 12:18:49 的 services open 均报告
   `Driver already in bad state`。

因此当前 `ENODEV` 的直接根因已确认为 shader 固件缺失导致的 RGX 初始化失败，不是 DMA/IOMMU
告警，也不是 `zink` 回退本身。当前安装的 `3.3.3.42-patched-18` 包仅含
`/lib/firmware/innogpu/fh2m.fw` 和 `fh2c.fw`；Deepin 202504 原包同时含有
`fh2m.sh` 和 `fh2c.sh`，而 patched-18 的历史混合载荷构建遗漏了这两个文件。驱动日志显示它会
优先尝试 `fh2m.sh`，所以后续修复应先完整保留 Deepin 202504 的 `.fw` 与 `.sh` 固件集合，再验证
是否仍需为 BVNC 版本名提供兼容别名；不得从其他补丁版本复制单个固件或 `.so`。

这次日志也解释了先前的相关性误判：`hwinfo_g0m.bin` 缺失仍是独立的可见错误，但本次 services
失败已经有明确的 shader `NOT_FOUND -> RGXInit` 因果链。`firmware_en=1` 暂不需要作为首要变量
单独回退；应先修复同源 shader 载荷，再重新运行隔离 Xorg、GBM/GLES2 和真实 VT 验证。

#### patched-20 诊断候选（已安装并越过 PVR 初始化门槛）

为区分 `hwinfo` 缺失、设备状态为 `BAD` 以及其他 PVR 初始化错误，新增
`patches/008-pvr-init-diagnostic.patch`。该补丁只在
`drm_pvr_srvkm_init()` 的 `DRM_IOCTL_PVR_SRVKM_INIT` 前后写入内核日志，记录
`init_module`、`priv`、`dev_node`、调用前后的 `eDevState` 和 services open 返回码；不改变
`PVRSRVDeviceServicesOpen()` 的调用参数、错误映射或用户态文件。

`scripts/build-patched20-deepin-diagnostic.sh` 以 Deepin 202504 原包为唯一载荷基础，启用
`APPLY_PVR_INIT_DIAGNOSTIC=1` 并生成 `3.3.3.42-patched-20`。补丁干跑、构建入口语法检查和离线
打包已完成；本机用户随后已安装该包并重启成功。安装、驱动替换和重启仍必须由本机用户执行，
SSH 会话不得自行执行这些动作。

本次离线构建已完成，包的 SHA-256 为
`747380492876d73fa72b5483ff0508ca0bd0a382efbc6c315fd6a6bf58ad23b6`。包内同时核对到
`.fb_mmap = fb_io_mmap` 和两条 `srvkm_init` 诊断日志；Deepin 202504 的 DRI、GBM、GLAPI 和
Xorg DDX 载荷未被替换，并保留 `fh2m.fw`、`fh2m.sh`、`fh2c.fw`、`fh2c.sh` 四个固件文件。
安装后的 `~/p20-kernel.log`（2026-08-14 13:29）提供了运行时证据：

1. `GPU00 Firmware image 'innogpu/fh2m.fw' loaded` 后，
   `GPU00 Shader binary image 'innogpu/fh2m.sh' loaded`，说明 Deepin 202504 的主固件和 shader
   固件均已进入当前系统；
2. 诊断日志中的 `srvkm_init ... state_before=2`、`ret=0 state_after=3` 表明 services 初始化
   成功，设备已进入 `PVRSRV_DEVICE_STATE_ACTIVE`（枚举值 3）；
3. 日志中未重新出现 `PVRSRVTQLoadShaders ... NOT_FOUND`、`RGXInit failed` 或
   `Driver already in bad state`。

因此 patched-20 已越过此前由 shader 固件缺失触发的 PVR 初始化阻塞。隔离 Xorg/GLX 和真实 VT
的 `fbterm` 均已在 patched-20 重启后通过。

该诊断补丁会记录每次 `DRM_IOCTL_PVR_SRVKM_INIT`，因此 `~/p20-kernel.log` 中可见重复的
`module=2 ret=0 state_after=3` 条目（本次日志为 128 次）。这属于诊断候选的预期副作用；完成
Xorg/GLX 验收后，正式包应移除该补丁或加入限速/仅首次记录，避免长期污染内核日志。

按厂商 `Makefile` 先运行 `cfg_detect.sh` 生成 `kernel_autocfg.h`，并创建各子目录的
`.o.cmd` 哨兵文件后，当前 `6.12.96+deb13-amd64` headers 下的临时编译已完成全部 C 编译和
链接阶段；最后的 `modpost` 因原包配置中的 `inno_gpu_info_init/exit` 未定义而停止。这是
Deepin 源码现有的模块配置/符号导出问题，不是 `008-pvr-init-diagnostic.patch` 引入的编译错误；
因此补丁本身已通过编译器，完整可加载 `.ko` 仍需复现正式 DKMS 的符号配置后再验证。

此前失败现场的日志由本机用户保存为 `~/p18-after-xorg-kernel.log`。patched-20 的后续复测可使用：

```sh
sudo journalctl -b -k --no-pager > ~/p20-kernel.log
rg -n -i 'PVR|PVRSRV|RGX|MMU|DMA|ENODEV|failed|bad' \
    ~/p20-kernel.log
```

patched-20 的诊断日志仍可用于记录 `state_before/state_after` 和 services 返回码，但不应替代
隔离 Xorg/GLX、正常桌面和真实 VT 验证，也不能通过 `firmware_en=1` 或 `hwinfo_g0m.bin` 的单变量
实验绕过同源固件验证。

patched-20 的隔离测试于 2026-08-14 13:36:23 完成，报告位于
`baselines/latest-current-xorg-hwgl-test/`，结果为：

1. Xorg `:9/vt8` 退出码 `0`；
2. `xdpyinfo` 和 `glxinfo` 返回码均为 `0`；
3. GLX 报告 `direct rendering: Yes`、`Accelerated: yes`，厂商为 `Innosilicon`，设备为
   `Fantasy II-M`，OpenGL 核心版本为 `4.3`，OpenGL ES 版本为 `3.2`。

因此此前 12:14:33 的 patched-18 退出码 134 仅保留为历史失败证据，不再代表当前 patched-20
运行结果。

真实 VT 的 `fbterm` 测试结果保存在 `~/fbterm-20p-result.txt`，内容为 `fbterm rc=0`。主进程
trace（`~/fbterm-p20.strace.95425`）显示：

1. `/dev/fb0` 以读写方式打开成功；
2. `FBIOGET_FSCREENINFO` 和 `FBIOGET_VSCREENINFO` 均返回 `0`；
3. `mmap(NULL, 16384000, PROT_READ|PROT_WRITE, MAP_SHARED, ...)` 返回有效地址；
4. `FBIOPAN_DISPLAY` 返回 `0`，进程正常退出，没有实际 `SIGSEGV`、`ENODEV` 或 `MAP_FAILED`。

测试时出现的 `[input] can't not change kernel keymap table` 警告来自普通用户对
`KDSKBENT` 的 `EPERM` 以及访问 `/dev/tty0` 的 `EACCES`。它只表示 fbterm 的内置滚屏和
切换 VT 快捷键无法写入内核键盘表，不影响 framebuffer 映射、文字绘制或正常退出；不应为消除该
提示而给 fbterm 增加不必要的全局特权。

## 验证状态

来源实现已验证：开盖热插、热拔、合盖外屏、再次开盖、开盖后再次拔出、stale geometry 清理、
扩展坞能力延迟、没有 preferred 的显式目标模式，以及 watcher 当前会话受控交接。

仓库吸纳后已重新验证：

- 基础状态和锁测试 12 项、watcher 生命周期 4 项、显示回归 11 项全部通过；
- 临时 HOME 中的用户安装器 4 项测试通过；
- patched-17 测试包从 patched-8 与 Deepin 202504 deb 完整重建，包内显示文件与仓库一致；
- patched-20 已从 Deepin 202504 完整载荷重建、安装并成功重启；`fh2m.sh` 已加载，
  `srvkm_init` 返回 0 且设备状态为 `ACTIVE`，此前的 shader 固件缺失阻塞不再出现；
- patched-20 隔离 Xorg/GLX 验收通过：Xorg、`xdpyinfo`、`glxinfo` 均成功，硬件直接渲染和加速
  已启用，渲染器为 Innosilicon Fantasy II-M；
- patched-20 真实 VT `fbterm` 验收通过：普通用户退出码为 0，`/dev/fb0` 映射和
  `FBIOPAN_DISPLAY` 均成功；仅保留无权限修改内核键盘映射的非致命警告；
- 当前 X11 中仓库与已安装 watcher 的 `--status` 输出一致，合盖外屏状态为 `health=ready`，
  无 stale 或 pending 输出；
- 当前 watcher 已运行来源相同的代码，因此本轮未做无意义的 watcher 热交接，也未重复执行会改变
  当前布局的热插拔、开合盖或 `--apply` 测试。实机 modeset 结论仍引用上述来源验证。

## 状态接口

当前命令接口为：

```text
xdisplay.sh [--apply]
xdisplay.sh --watch
xdisplay.sh --status
```

`--status` 必须只读。来源计划中的 `--manual-run`、manual marker 和单设备适配器尚未完成，不能在
本项目文档中提前声明为可用。
