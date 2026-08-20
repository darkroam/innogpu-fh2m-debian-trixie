# FH2M 能力普查记录

## 状态

- 本记录对应 [reverse-engineering-assessment.md](reverse-engineering-assessment.md) 的"能力挖掘任务"
  P0/P1 项。**静态与运行时两部分均已执行**：静态部分在本容器内完成；运行时部分由真实会话运行
  `scripts/run-capability-survey.sh` 完成（vulkaninfo/clinfo/vainfo/drm_info 权威工具 + 最小探针
  交叉验证），结果见"运行时结果"节。
- 剩余运行时补充项（DVFS/功耗、CORE_ID 直接读取、私有 codec 编码接口）见"运行时待办"。
- 原始探针输出保留在 `baselines/capability-survey-*.log`（被 .gitignore 忽略），不进入 Git；
  本文件只记录精简结论。

## 方法与环境边界

- 静态勘察对象：Deepin 202504 原包解包树 `third_party/innogpu-fh2m-deepin-202504/root`、
  系统 `/usr/lib/x86_64-linux-gnu/innogpu-fh2m/`、`/sys/class/drm/card0`、`lspci`。
- 探针工具：`tools/probe-vulkan-devices.c`（dlopen loader 枚举）、`tools/probe-opencl-devices.c`
  （dlopen ICD loader 枚举），运行入口 `scripts/run-capability-survey.sh`。
- 容器内所有探针只读、不 modeset、不修改系统配置。

## 硬件架构（sysfs/PCI 证据）

| 项 | 值 | 证据 |
| --- | --- | --- |
| GPU | Beijing Fantasy Technology Co., Ltd. Fantasy II-M | `lspci -nn`：`02:00.0 [1ec8:9810]`，class 0x030000 |
| 内核驱动 | `inno-drv`（模块 `innogpu`） | `lspci -vv`：Kernel driver in use: inno-drv |
| IRQ | MSI IRQ 74 | `lspci -vv`；与既有运行证据一致 |
| PCI BAR | BAR0 64M 非预取；BAR2 512M 预取；BAR4 I/O 256B；Expansion ROM 512K | `lspci -vv` |
| VRAM | **2 GiB**（`0x80000000`） | `/sys/class/drm/card0/device/gpu-info` |
| 平台子设备 | 16 个：`inno-gpu`、`inno-drm`、`inno-dma`、`inno-pdma`、`inno-codec`、`inno-audio`、`inno-pmbus`×2、`inno-power`、`inno-pdp-dp`、`inno-pdp-dpu`×4、`inno-pdp-hdmi`×2、`inno-pdp-vga` | `/sys/class/drm/card0/device/` |
| connector | eDP-1 disconnected、HDMI-A-1 disconnected、HDMI-A-2 connected | `/sys/class/drm/card0/*/status` |

说明：PCI BAR2 仅 512M 而 VRAM 2 GiB，即 small-bar 窗口；这解释了驱动内置
`SYS2GDDR`/`GDDR2SYS` DMA 搬运路径（`innodma.o_shipped` 的
`fh2m_innodma_memcpy_for_smallbar_sg`，见
[webkit-dmabuf-investigation.md](webkit-dmabuf-investigation.md)）存在的必要性。

## 内核驱动谱系（决定"谱系重构"可行性的核心证据）

| 项 | 值 | 证据 |
| --- | --- | --- |
| DDK 基线 | Imagination PowerVR DDK **V119 RTM** 构建管线 | `innovpu.o_shipped` 内嵌路径 `DDK_V119RTM_RELEASE_BUILD_PIPELINE_DDK/ddk/kmd/...` |
| BVNC | **35 / V / 1632 / 23** | `innosrvkm/include/configs/rgxconfig_km_35.V.1632.23.h`：`RGX_BNC_KM_B 35`、`RGX_BNC_KM_N 1632`、`RGX_BNC_KM_C 23` |
| 芯片变体 | **G0M_SOC**（1 SPU / 2 clusters） | `config_kernel.h` 第 14 行 `#define __G0M_SOC__`；shipped 对象含 `CHIP_G0M_SOC/NE/PAL`；与缺失的 `hwinfo_g0m.bin` 线索一致 |
| 固件处理器 | META MTP219 | `RGX_FEATURE_META (MTP219)`，META_COREMEM 8 banks × 96，DMA 4 通道 |
| 特性定义 | **90 个 RGX_FEATURE 宏**（含数值） | 同一 config 头文件 |

关键特性数值（G0M_SOC 分支生效）：

- 计算：`COMPUTE`、`COMPUTE_OVERLAP(_WITH_BARRIERS)`、`COMPUTE_MORTON_CAPABLE`、`COMPUTE_SLC_MMU_AUTO_CACHE_OPS`
- 几何/光栅：`TESSELLATION`、`SCALABLE_TE_ARCH=2`、`SCALABLE_VCE=2`、`SCALABLE_VDM_GPP`、`VDM_DRAWINDIRECT`、`FASTRENDER_DM`、`TILE_SIZE 32×32`、`ZLS_CHECKSUM`
- 纹理：`MAX_TPU_PER_SPU=2`、`BINDLESS_IMAGE_AND_TEXTURE_STATE`、`TPU_CEM/DM_GLOBAL_REGISTERS`
- 压缩：`FBCDC=4`、`FBCDC_ALGORITHM=4`、`FBCDC_ARCHITECTURE=6`、`FBC_MAX_DEFAULT_DESCRIPTORS=2048`、`FBC_MAX_LARGE_DESCRIPTORS=32`
- 内存：`SLC_SIZE=512KB`、`SLC_BANKS=4`、`SLC_CACHE_LINE_SIZE_BITS=1024`、`SLC_VIVT`、`MMU_VERSION=4`、`PHYS_BUS_WIDTH=40`、`VIRTUAL_ADDRESS_SPACE_BITS=40`
- 体系：`AXI_ACE`（硬件缓存一致性）、`ALBIORIX_TOP_INFRASTRUCTURE`、`BARREX_TOP_INFRASTRUCTURE`、`S7_TOP_INFRASTRUCTURE`、`GPU_VIRTUALISATION`、`GPU_MULTICORE_SUPPORT`、`SIGNAL_SNOOPING`、`SYS_BUS_SECURE_RESET`、`SOC_TIMER`、`PERFBUS`、`PERF_COUNTER_BATCH`
- 不在硬件中：`RAY_TRACING_ARCH=0`（无光追）、`ECC_RAMS=0`（无 ECC）、`SPU0-3_RAC_PRESENT=0`、`LAYOUT_MARS=0`

结论：内核侧是"标准 Rogue 世代（含 Albiorix/S7 标记）的 DDK V119 + Innosilicon G0M HAL 插件"，
与 [评估文档](reverse-engineering-assessment.md) 的"开源谱系重构"判断一致；G0M 芯片配置
（1 SPU / 2 cluster）已由编译配置确定，无需运行时读取。

## 用户态能力（静态字符串/符号证据）

### Vulkan（`libVK_INNO.so`）

- ICD manifest：`/etc/vulkan/icd.d/innoconf.json` → `libVK_INNO.so`，api_version **1.3.264**
- 导出：`vk_icdNegotiateLoaderICDInterfaceVersion`、`vk_icdGetInstanceProcAddr`、`vk_icdGetPhysicalDeviceProcAddr` 齐备
- 扩展字符串：**128 个唯一扩展**（61 EXT + 64 KHR + 3 厂商：`VK_ARM_rasterization_order_attachment_access`、
  `VK_IMG_conditional_rendering_comparison_info`、`VK_IMG_format_pvrtc`）。注：此为二进制字符串
  去重后的下界，权威列表以运行时 `vkEnumerateDeviceExtensionProperties` 为准（待实机）
- 代表性扩展：`VK_KHR_dynamic_rendering`、`VK_KHR_timeline_semaphore`、`VK_KHR_synchronization2`、
  `VK_KHR_external_memory/fence/semaphore_fd`（dma-buf 互操作）、`VK_KHR_push_descriptor`、
  `VK_KHR_shader_float16_int8`、`VK_KHR_sampler_ycbcr_conversion`、`VK_EXT_descriptor_indexing`、
  `VK_EXT_descriptor_buffer`、`VK_EXT_external_memory_dma_buf`、`VK_EXT_memory_budget`、
  `VK_EXT_graphics_pipeline_library`、`VK_EXT_subgroup_size_control`、`VK_IMG_format_pvrtc`
  （PVRTC 纹理，PowerVR 签名格式）、`VK_ARM_rasterization_order_attachment_access`

### OpenCL（`libINNOOCL.so`）

- ICD：`/etc/OpenCL/vendors/INNO.icd` → `libINNOOCL.so`；导出完整 cl* 入口
- 版本：**OpenCL 3.0** / OpenCL C 3.0
- 代表性扩展：`cl_khr_command_buffer`、`cl_khr_fp16`、`cl_khr_integer_dot_product`、
  `cl_khr_il_program`（SPIR-V IL）、`cl_khr_external_memory_dma_buf`、`cl_arm_import_memory_dma_buf`、
  `cl_arm_scheduling_controls`、`cl_khr_subgroups` 及 subgroup 全套、`cl_khr_semaphore`、
  `cl_khr_egl_image`、`cl_khr_3d_image_writes`、`cl_img_yuv_image` 等

### 视频编解码（`libinno_codec.so` + `innogpu_drv_video.so`）

- `innogpu_drv_video.so` 是 **VA-API 驱动**（`inno_libva_decode/encode/caps/format.cc`）
- 解码：`InnoVaDecodeAVC`（H.264）、H.265 SPS 解析符号存在
- 编码：`InnoVaEncodeAvc`（H.264）；codec 库内还有 `WaveDecoder`、`Wave627Encoder`、`Wave677Encoder`
- 结论：FH2M 带硬件视频解码/编码用户态（至少 H.264 编解、H.265 解），能力面尚未被本项目验证

### 其他用户态

- IFBC：`libifbc.so` 导出 `ifbc_convert`、`ifbc_convert_get_feature/set_feature`（帧缓冲压缩转换）
- DDX：`innogpu_drv.so` 为 glamor 风格、内嵌 GLSL（DRI2/DRM/glamor）
- GL 栈：Mesa 派生 `libGL_INNO_MESA.so`/GLESv2/GLX/glapi + `libinno_mesa_wsi.so`
- GBM：`libinnogpu_gbm.so`（backend 名 `inno`，与既有 GBM 探针一致）

## 固件

- `fh2m.fw`/`fh2c.fw`（约 135KB）：META 固件，格式 `GLS_BINARY_LSB_FIRST`，头 `00 55 aa 01`
- `fh2m.sh`/`fh2c.sh`（约 534KB）：USC 编译器产出的 shader 固件
- 均为闭源 blob，作为固定输入处理，不做逆向（见 [评估文档](reverse-engineering-assessment.md) 边界）

## Vulkan ICD 加载调查（完整结论）

容器内从"loader 找不到驱动"现象出发，逐层验证（loader 1.4.309 源码对照 + strace + 直接调用 ICD）：

1. **ICD 可加载**：`dlopen("libVK_INNO.so")` 成功，依赖（`libsrv_um_inno.so` 等）经
   `/etc/ld.so.conf.d/0-innogpu.conf` 解析正常；
2. **manifest 发现正常**：默认 `/etc/vulkan/icd.d/innoconf.json` 相对路径 `libVK_INNO.so`
   能被 loader 找到并 dlopen（strace 确认）；
3. **接口协商正常**：loader 1.4.309 请求接口版本 7（假 ICD 记录确认）；ICD 对版本 1/2 返回
   `VK_ERROR_INCOMPATIBLE_DRIVER`，对 3–7 返回成功并将返回版本封顶为 7——符合协商协议；
4. **失败点唯一**：ICD 的 `vkCreateInstance` 在无 `/dev/dri` 时返回
   `VK_ERROR_INITIALIZATION_FAILED`，loader 据此丢弃该 ICD 并报 `Found no drivers`。

结论：**Vulkan ICD 本身有效且与标准 loader 兼容，失败仅因容器无 DRM render 节点**。真实设备上
Vulkan 应可枚举 Fantasy II-M；需在真实会话用 `scripts/run-capability-survey.sh` 确认（待办）。

## 运行时结果（2026-08-20 真实会话，多份 baselines/capability-survey-*.log，最新 135901）

### GLX（用户 glxinfo -B）

- Renderer：Innosilicon / Fantasy II-M；direct rendering Yes；Accelerated yes
- **OpenGL 4.3 core**、compat 3.0、GLES 3.2、GLES1 1.1（Mesa 派生栈版本 23.1.3）
- Video memory：**1996MB**（与 gpu-info 2 GiB 一致，减去保留部分）

### Vulkan（vulkaninfo --summary，多次运行一致）

- **GPU0 = Fantasy II-M**：apiVersion 1.3.264，vendorID 0x1ec8，deviceID 0x35020023，
  **DISCRETE_GPU**，conformance **1.3.7.0**
- driverID = DRIVER_ID_IMAGINATION_PROPRIETARY；driverName = **"InnoGPU B-Series Vulkan Driver"**
  （确认 B 系列，与 BVNC B=35 一致）；driverInfo = 23.3@88877759
- deviceUUID 前 13 字节为 ASCII 文本 `"35 4- 1632 23"`（含 BVNC 数字 35/1632/23），与编译配置互相印证
- GPU1 = llvmpipe（CPU 回退，正常存在）
- 容器诊断结论验证：**ICD 有效，实机开箱即可枚举**

### OpenCL（clinfo，多次运行一致）

- 平台：**InnoGPU / Innosilicon / OpenCL 3.0 / EMBEDDED_PROFILE**
- 设备：**Fantasy II-M**、**Max compute units 2**（与 G0M_SOC 1 SPU/2 clusters 一致）、
  **Max clock 1349 MHz**
- OpenCL C 3.0 全特性：device_enqueue、pipes、subgroups、generic address space、int64、
  integer dot product 4x8bit（2.0 特性）；**fp16 硬件支持、无 double**（fp64 n/a）
- 扩展：cl_khr_il_program（SPIR-V）、cl_khr_command_buffer、dma-buf 导入（khr+arm）、subgroup 全套
- **conformance：v2021-10-04-00 通过**；Driver Version 23.3@88877759（与 Vulkan 同 DDK）

### VA-API（vainfo 权威 + 最小探针交叉验证）

- 驱动：/usr/lib/x86_64-linux-gnu/dri/innogpu_drv_video.so，INNO-silicon Driver v1.0.0，libva 1.22
- **硬解**：H264Main / H264High / H264ConstrainedBaseline / HEVCMain / HEVCMain10 全部
  为 VAEntrypointVLD；另附 VAEntrypointStats（统计）
- **后处理**：VAProfileNone 提供 VideoProc + Stats
- **编码**：无任何 EncSlice 或 EncPicture entrypoint——**VA-API 只暴露解码**；编码符号
  （InnoVaEncodeAvc、Wave627/677 Encoder）存在于私有 libinno_codec.so，实机可用性未验证
  （可能走私有接口而非 VA-API）
- 注：最小探针首次 query 返回 status=18（驱动不支持"先查数量"模式），已修复并交叉验证一致；
  entrypoint 编号按权威 libva 枚举映射（12=Stats）

### KMS / DRM（drm_info）

- 驱动：innogpu（Innosilicon Technologies Gpu Driver）**version 2.19.88877759**（构建号与
  Vulkan/OpenCL 的 23.3@88877759 一致）
- 能力：**DRM_CAP_PRIME=3**（导入+导出）、ADDFB2_MODIFIERS=1、ATOMIC、UNIVERSAL_PLANES、
  WRITEBACK_CONNECTORS、VBLANK_HIGH_CRTC；**SYNCOBJ=0 / SYNCOBJ_TIMELINE=0**（无显式同步对象，
  关联 [webkit-dmabuf 调查](webkit-dmabuf-investigation.md) 的隐式同步路径）
- 拓扑：3 个 CRTC、8 个 plane；connector 0=eDP（disconnected）、1=HDMI-A（disconnected）、
  2=HDMI-A（connected，600x330mm，preferred 1920x1080@60，最高 1080p75）
- framebuffer 上限 16384x16384；cursor 64x64

## 运行时待办

- [ ] DVFS/功耗实测：/sys/class/drm/card0/device/ 下电源管理节点与 BMC 符号（部分 sysfs 已抓取：
      power=runtime active）
- [ ] 运行时读取 CORE_ID/BVNC 直接核对编译配置（deviceUUID 已间接印证 35/1632/23）
- [ ] 私有 libinno_codec.so 编码接口的实机验证（非 VA-API 路径，需独立探针）

## 证据保留

- 静态证据全部来自仓库内 Deepin 202504 解包树与 sysfs，可复现；
- 原始探针日志在 `baselines/capability-survey-*.log`（gitignore），不入 Git；
- 本文件的结论不修改任何驱动或用户态载荷。
