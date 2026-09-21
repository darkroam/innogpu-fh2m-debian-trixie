# DDK V119 ↔ 开源参照对照表

## 状态

- 本记录对应 [reverse-engineering-assessment.md](reverse-engineering-assessment.md) 的"中期谱系重构"
  任务，基于静态勘察与上游公开源码对比，**未修改任何驱动或载荷**。
- 目的：把 Deepin 202504 驱动栈（DDK V119 谱系）与可获取的开源参照逐组件映射，
  回答"哪些部分可以对照开源还原语义、哪些是 Inno 自有、迁移到主线需要什么"。

## 一、谱系判定（证据）

| 证据 | 结论 |
| --- | --- |
| `innovpu.o_shipped` 内嵌构建路径 `DDK_V119RTM_RELEASE_BUILD_PIPELINE_DDK/ddk/kmd/...` | Imagination PowerVR **DDK V119 RTM** 构建管线 |
| `innosrvkm.o_shipped` 符号：`RGX_FEATURE_*`、`asRGXFWLayoutTable`、`APPHINT_ID_RGXBVNC`、`fh2m_RGXDevBVNCString` | 标准 Rogue（RGX）services 核心 + Inno 设备适配 |
| 源码头 `rgxconfig_km_35.V.1632.23.h`（`RGX_BNC_KM_B 35 / N 1632 / C 23`） | 目标硬件配置 **35.V.1632.23** |
| `config_kernel.h` 的 `__G0M_SOC__` | 编译变体 G0M_SOC（1 SPU / 2 clusters） |
| `fh2m.fw` 格式 `GLS_BINARY_LSB_FIRST`、`RGX_FEATURE_META (MTP219)` | 固件跑在 Imagination META 处理器上 |
| `libVK_INNO.so` 的 `DRIVER_ID_IMAGINATION_PROPRIETARY`、driverName "InnoGPU B-Series" | 用户态 Vulkan 也是 Imagination 谱系 |

结论：**内核与用户态主体是 DDK V119（Rogue 世代）services 栈 + Innosilicon 定制**；
B 系列（BXS/Series3NX 世代）与 BVNC B=35 一致。

## 二、内核组件映射表

| shipped 文件 | 角色（符号证据） | DDK 对应层 | 开源参照 | 开源可用性 |
| --- | --- | --- | --- | --- |
| `pvr_platform_drv.c` | services 设备生命周期（`PVRSRVCommonDeviceCreate/Initialise`） | services/server/devices | Fuchsia `imgtec-pvr-rgx-km`（同谱系 KM）；主线 `pvr_drv.c`（新架构） | 部分 |
| `pvr_bridge_k.c` | services 桥接分发（`BridgedDispatchKM`、`BridgeDispatchTable`） | services/bridge | Fuchsia KM（近似）；主线无此层 | 部分 |
| `pvr_drm.c` | 旧式 DRM 包装（`drm_load/drm_ioctl`） | DDK DRM 层 | 主线 `pvr_drv.c`（架构不同） | 概念对照 |
| `pvr_buffer_sync.c` | dma-buf/resv 同步（`dma_resv_*` 现代 API） | services/km | 主线 `pvr_sync.c`/`pvr_job.c` | 概念对照 |
| `pvr_fence.c`/`pvr_sw_fence.c`/`pvr_export_fence.c`/`pvr_counting_timeline.c`/`pvr_sync_file.c` | dma_fence/时间线/同步原语 | services/km | 主线 `pvr_sync.c`（fence 模型不同） | 概念对照 |
| `pvr_dvfs_device.c` | DVFS（`BMC_GPU_FREQ` 等符号） | services/km | 主线 `pvr_power.c`（devfreq 模型） | 概念对照 |
| `pvr_procfs.c`/`pvr_debugfs.c`/`pvr_gputrace.c` | 调试/跟踪 | services/km | 主线 `pvr_debugfs.c` | 概念对照 |
| `innosrvkm.o_shipped` | **services 核心**（RGX 初始化、MMU、固件加载、fence 核心） | services（编译态） | Fuchsia KM 同谱系；主线 `pvr_fw.c`/`pvr_mmu.c` | 需对照还原 |
| `innogpu.o_shipped` | Inno FH2M HAL（`fh2m_hal_*`、BMC、efuse、PLL、隔离复位） | Inno 自有 | 无 | 无 |
| `innodma.o_shipped`/`innosmmu.o_shipped` | Inno DMA/SMMU 引擎 | Inno 自有 | 无（`hal_dma.c` 有调用方源码） | 无 |
| `innovpu.o_shipped` + `innovpu_for_umd.h` | VPU 驱动（VDI ioctl：物理内存/中断/时钟门控） | Inno 自有 | 无（UMD 接口头开源） | 无 |
| `innodpu_*.c`/`pdp0_*.c`/`g3_*.c`（约 40 个源文件） | **DPU 显示控制器**（CRTC/plane/connector/DP/HDMI/VGA/fbdev） | Inno 自有 | 无（本项目已在源码层优化） | 完整源码 |

## 三、UAPI 对比

| 维度 | shipped（services 栈） | 主线 drm/imagination（v6.12） |
| --- | --- | --- |
| ioctl 族 | `DRM_IOCTL_PDP_GEM_*`（CREATE/MMAP/CPU_PREP/CPU_FINI/OBJ_FD/CHIP_INFO 等）+ services bridge + VDI | `DRM_PVR_*`（BO/VM/queue/job/stream/heap/context/sync/device query） |
| 内存模型 | PDP GEM + visible/invisible VRAM + GTT | `pvr_bo` + `pvr_vm`（无 visible/invisible 概念） |
| 调度 | services 固件命令流 | 用户态 queue + CCB/stream（新模型） |
| 同步 | dma_fence + 自定义 sync ioctl | `pvr_sync`（drm syncobj 风格） |
| 固件要求 | fh2m.fw（META）由 services 加载 | `pvr_fw.c` 要求匹配内核预期格式 |

**结论：两套 UAPI 完全不兼容；迁移到主线 = 用户态全重写 + 固件接口重做，不是增量路径。**

## 四、特性/配置对照

- shipped：`rgxconfig_km_35.V.1632.23.h`（90 个 RGX_FEATURE 宏，G0M_SOC：1 SPU/2 clusters、
  FBCDC v4、512KB SLC、META MTP219、无光追/ECC）。
- 主线 v6.12：`pvr_device_info.c` 通过固件 feature/quirks 位图校验，**未包含 35.V.1632.23 配置**
  （grep 无 1632/BVNC 匹配）。主线支持的设备集为另一批 Rogue/BXS 型号。
- Mesa pvr（Vulkan）：与主线 drm/imagination 配对，同样只支持主线已支持设备。
- 含义：即便走迁移路线，还需在主线添加 35.V.1632.23 的配置/固件适配——超出本项目范围，属上游工程。

## 五、用户态对照

| shipped 用户态 | 角色 | 开源参照 | 可用性 |
| --- | --- | --- | --- |
| `libsrv_um_inno.so`/`libusc_inno.so`/`libufwriter_inno.so` | services UMD / USC 编译器 / UF writer | 开源 DDK UMD 已不可得；Mesa pvr 是新实现 | 无 |
| `libVK_INNO.so`（1.3.264） | Vulkan ICD（Imagination 谱系） | Mesa `src/imagination/vulkan`（仅对新 UAPI） | 概念对照 |
| `libINNOOCL.so`（OpenCL 3.0） | OpenCL ICD | 无开源 OpenCL for PowerVR | 无 |
| `libGL_INNO_MESA.so` 等 | Mesa 派生 GL/EGL/GLX | Mesa 上游（同版本可比对） | 可对照 |
| `libinnogpu_gbm.so`/`inno_dri.so`/`innogpu_drv.so` | GBM/DRI/DDX | Mesa gbm/dri + glamor | 可对照 |
| `libinno_codec.so`/`innogpu_drv_video.so` | 视频编解码（VA-API 解） | 无 | 无 |

## 六、结论与路线

1. **services 架构内继续源码层优化**（当前路径）：DPU 层完整源码、services 包装层部分源码，
   `innosrvkm.o_shipped` 核心以"对照 Fuchsia KM 语义还原为文档 + 可替换源码"方式积累。
   —— 可行且已在进行（patch-025/026/027 均为该路径成果）。
2. **迁移主线 drm/imagination + Mesa pvr**：需要 Inno 提供与主线匹配的固件、主线添加
   35.V.1632.23 配置、用户态全重写。门槛高，属长期上游工程，非本项目可独立完成。
3. **Inno 自有组件**（G0M HAL、DMA/SMMU/VPU、codec、DPU）无开源参照，只能：
   - DPU/接口层源码化（已完成、可继续优化）；
   - HAL/DMA 做符号级还原（远期，产出 ABI 文档与可替换源码）。

## 七、证据来源与限制

- shipped 侧：Deepin 202504 解包树（源码 + 预编译对象符号/字符串）。
- 上游侧：Linux v6.12 `drivers/gpu/drm/imagination`（git.kernel.org 获取）；
  Mesa `src/imagination`（gitlab.freedesktop.org）。
- 限制：2019 年开源 DDK 仓库 `powervr-rogue-drivers/powervr-graphics` 已下线；
  Fuchsia `imgtec-pvr-rgx-km`（同谱系 Rogue KM）从本环境不可达（googlesource 超时）。
  若未来取得上述同谱系源码，可把第二节"部分"升级为逐函数对照。
