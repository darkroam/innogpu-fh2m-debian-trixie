# 逆向工程与能力挖掘评估

## 状态

- 本文件是 FH2M 驱动"逆向重建 / 能力挖掘 / 逻辑优化"的评估与任务计划。除能力普查的静态部分
  已执行并记录于 [capability-survey.md](capability-survey.md) 外，其余内容均为未实施计划，
  不描述任何当前已验证行为。
- 评估基于 Deepin 202504 原包解包树（`third_party/innogpu-fh2m-deepin-202504/root`）的静态勘察，
  以及既有运行证据（[webkit-dmabuf-investigation.md](webkit-dmabuf-investigation.md)、
  [patched-24 验收](../patches/patched-24-kernel-612101.md)）。
- 任务状态由 [todo.md](todo.md) 跟踪；当前运行状态以 [status.md](../project/status.md) 为唯一摘要。

## 背景与结论摘要

原始问题：FH2M 显卡驱动未开源，是否可以通过逆向工程或其他手段自己重建驱动、持续挖掘硬件能力、
并优化处理逻辑。

评估结论：

| 子目标 | 可行性 | 一句话依据 |
| --- | --- | --- |
| 全量黑盒重建整个驱动栈 | 不可行 | 工作量相当于商业 GPU 驱动团队多年积累，且固件层为纯黑盒 |
| 将内核驱动"重构"为可维护源码 | 高 | 驱动栈大量派生自 Imagination 开源 PowerVR DDK，可对照开源实现还原语义 |
| 持续挖掘 FH2 硬件能力 | 高 | 包内自带 Vulkan 1.3.264 与 OpenCL ICD；内核 RGX 特性表可 dump |
| 优化 FH2M 处理逻辑 | 中—高 | 已有多个已定位候选，多数落在源码层 |

## 驱动栈构成（静态勘察证据）

### 四层结构

| 层 | 内容 | 源码状态 |
| --- | --- | --- |
| 显示/DPU 内核驱动 | `innodpu_*.c`、`pdp0_*.c`、`g3_*.c`、`innodpu_drm_gem.c` 等 | 完整源码 |
| PVR services 内核 | `pvr_*.c`、`inno_apphint.c` 等包装 + `innosrvkm.o_shipped` 预编译核心 | 源码 + 预编译 |
| Inno 自有硬件层 | `innogpu.o_shipped`（FH2M HAL）、`innodma.o_shipped`（DMA）、`innosmmu.o_shipped`（SMMU）、`innovpu.o_shipped`（VPU） | 纯预编译 |
| 用户态 | DDK UMD（`libsrv_um_inno.so`、`libusc_inno.so`、`libufwriter_inno.so`）、Mesa 派生 GL 栈（`libGL_INNO_MESA.so` 等）、`libinnogpu_gbm.so`、DRI、DDX、`libVK_INNO.so`、`libINNOOCL.so`、`libifbc.so`、`libinno_codec.so` | 纯预编译 |

### 决定可行性判断的关键证据

1. `innosrvkm.o_shipped` 是标准 Imagination Rogue（RGX）services 代码：符号表含
   `RGX_FEATURE_*` 特性枚举（`RGX_FEATURE_META`、`MMU_VERSION`、`NUM_CLUSTERS`、`FBCDC`…）、
   `asRGXFWLayoutTable`、`APPHINT_ID_RGXBVNC`。
2. `innovpu.o_shipped` 内嵌构建路径 `DDK_V119RTM_RELEASE_BUILD_PIPELINE_DDK/ddk/kmd/innogpu/hal`
   与 `rogue_heap_memory_*` 符号：说明内核驱动来自 Imagination PowerVR DDK V119 RTM 构建管线 +
   Innosilicon 的 `innogpu` HAL 插件。
3. 固件 `fh2m.fw`/`fh2c.fw` 为跑在 Imagination META 微处理器上的微码；`fh2m.sh`/`fh2c.sh`
   （约 534KB）为 USC 编译器产出的 shader 固件（见
   [shader 固件事故记录](../incidents/patched-18-shader-firmware.md)）。
4. 用户态 ICD：`/etc/vulkan/icd.d/innoconf.json` 指向 `libVK_INNO.so`（Vulkan 1.3.264）；
   `/etc/OpenCL/vendors/INNO.icd` 指向 `libINNOOCL.so`。两套 ICD 随包提供，说明硬件能力面包含
   Vulkan 与 OpenCL compute，本项目尚未系统触达。

## 可行性分层评估

### 1. 全量黑盒重建：不可行

寄存器手册、固件 ISA（META + USC）均不在手；完整 GPU 驱动还包含调度、同步、内存管理与编译器，
量级超出本项目可承受范围，也不符合维护策略的载荷纪律。

### 2. 开源谱系重构：最值得投入（未实施）

不是从零逆向，而是对照开源参照实现"翻译"预编译对象：

- `innosrvkm.o_shipped` 与 Imagination 开源 `pvrsrvkm`（`powervr-graphics`，MIT）、Linux 主线
  `drivers/gpu/drm/pvr`、Mesa `pvr` 驱动同源。以 DDK V119 对应开源 tag 做对照，可把设备初始化、
  RGX 特性表、MMU、fence、DVFS 语义还原为可维护文档与源码。
- 用户态 `libsrv_um`/`libusc` 对应开源 DDK UMD；Mesa 派生 GL 栈对应 Mesa 同版本源码，可做
  能力面对照。
- 前置动作：确认 FH2M GPU 核心型号/BVNC（`APPHINT_ID_RGXBVNC` 可从 shipped 对象读出）。
  若确认为标准 Rogue/BXS 核心，`innosrvkm.o_shipped` 存在被开源构建替换的远期可能性。

### 3. 定向接口 RE：可行，已有先例（未实施）

对 Inno 自有部分（`innogpu.o_shipped` HAL、`innodma.o_shipped`）做符号级分析
（`objdump`/Ghidra + 对照开源 DDK HAL 结构），产出 ABI 文档与可替换源码，而非直接修改二进制。
项目已有同类先例：`tools/patch-gpupll-object.py`（严格字节契约对象变换，stage-000）、
`tools/trace-loader.c`（LD_PRELOAD 诊断 shim）、`tools/probe-pdp-invisible-read.c`（私有 PDP ioctl
探针）；工具清单见 [tools/README.md](../../tools/README.md)。

### 4. 用户态重写：不可行，但可"借道"

重写 GL/Vulkan/OpenCL 用户态等于重写半个 Mesa，不做。远期可评估：若内核 ioctl 层收敛到接近
主线 `pvr` UAPI，接入主线 Mesa `pvr` 驱动的可行性。这是远期选项，不是当前任务。

## 能力挖掘任务（未实施）

| 优先级 | 任务 | 说明 |
| --- | --- | --- |
| P0 | Vulkan 能力普查 | 隔离 ICD 环境运行 `vulkaninfo`，枚举 API 版本、扩展、队列族、feature 上界 |
| P0 | OpenCL 能力普查 | 隔离 ICD 环境运行 `clinfo`，确认设备名、版本、双精度、image 支持 |
| P0 | RGX 特性表 dump | 读取 `aui16_RGX_FEATURE_*` 表（FBCDC、MMU、META、cluster/TPU/SPU、ECC），产出硬件 IP 配置单 |
| P1 | BVNC/核心型号确认 | 从 `innosrvkm.o_shipped` 读出 `APPHINT_ID_RGXBVNC`，作为开源谱系对照锚点 |
| P1 | 视频编解码能力确认 | 查清 `libinno_codec.so` 与 `innogpu_drv_video.so` 接 VA-API/Xv 还是私有接口 |
| P1 | IFBC 验证 | 确认 `libifbc.so` 帧缓冲压缩是否在扫描路径启用 |
| P1 | DVFS/功耗实测 | `pvr_dvfs_device.c` 为源码；`BMC_GPU_FREQ/POWER/VOLTAGE` 符号可读，实测频率/功耗/温度曲线 |
| P2 | 未完成矩阵 | `hwinfo_g0m.bin` 影响、电源/合盖/拔屏/多屏矩阵、跨硬件验证（见 [status.md](../project/status.md)） |

## 优化候选（未实施）

### 已定位、可直接立项（源码层，无需逆向）

1. `inno_gem_object_cpu_prep_ioctl()` 未用 `dma_resv_usage_rw(write)` 转换 READ/WRITE 语义；
   静态审计已定位（见 [webkit-dmabuf-investigation.md](webkit-dmabuf-investigation.md)）。
2. `pdp0_crtc_enable_vblank()` 对未活动 CRTC 无条件置位并返回成功，可让错误用户态永久阻塞。
3. foreign DMA-BUF import 未处理 `dma_buf_attach()` error pointer；GTT export 缺
   `dma_unmap_page()`。
4. invisible READ 批量预取：page fault 路径 `innodpu_gem_invisible_vram_vm_fault` 在源码，
   可在调用方按页窗口批量提交 `GDDR2SYS`，不必修改 `innodma.o_shipped` 内部。p23 已把 READ
   `munmap` 成本降约 97%，剩余 0.06–0.07ms/page 的 fault 搬运是主战场。
5. `inno_apphint.c` 为源码：对照 DDK apphint 文档逐项评估用户态调优。

### 基于谱系对照的新增机会

6. 确认 DDK V119 对应开源 tag 后，移植上游已知 bugfix/性能 patch。
7. DVFS 调参：频率/电压表符号可读，配合 BMC 实测做功耗-性能权衡。
8. 用户态调用画像：扩展 `trace-loader.c` 到 GL/VK/OCL 路径，定位下一个热点。

## 边界与风险

- **许可**：源码层为 MIT/GPLv2 双许可（如 `innovpu_for_umd.h`）；预编译对象与固件为闭源载荷。
  反汇编用于互操作存在法域差异。**逆向产出以文档 + 可替换源码 + 字节契约工具形式存在**，
  不直接修改闭源二进制，符合 [维护策略](../project/maintenance-policy.md) 的载荷纪律。
- **固件不值得逆向**：META 微码 + USC shader 固件是 Imagination 工具链产物，作为固定输入处理。
- **风险控制**：内核改动一律遵循现有流程（离线 DKMS 编译 → 不热切换 → 重启验证 → 回退链
  `patched-24 -> patched-21 -> patched-17 -> patched-8`）；逆向工作只产生证据与源码，不产生新的
  发布载荷。

## 推进路线

### 短期（能力面普查，低风险）

1. 能力面普查（见"能力挖掘任务"P0/P1）。
2. 落地静态审计已定位的内核接口修复（优化候选 1–3），各自独立 patch，遵守"一个补丁一个变量"。

### 中期（谱系重构）

3. 建立 DDK V119 ↔ 开源 `pvrsrvkm`/Mesa 的逐文件对照表，产出 `innosrvkm.o_shipped` 语义还原文档。
4. 评估 invisible READ 批量预取候选（优化候选 4）。

### 远期（定向 RE）

5. 对 `innogpu.o_shipped`（HAL）与 `innodma.o_shipped`（DMA）做符号级分析，目标是以
   "开源谱系 + 还原 HAL"组合替换预编译核心。

## 验证与回退

- 每个实施项遵循"文档计划 → 代码/补丁 → 与风险相称的测试 → 文档复核 → 提交"顺序
  （见 [维护策略](../project/maintenance-policy.md)）。
- 探针类工作复用 [tools/README.md](../../tools/README.md) 现有约束（不 modeset、不修改系统配置）。
- 内核补丁必须分别通过离线 DKMS 编译与实机重启验证，结果写入对应 `docs/patches/` 记录；
  失败项进入 [todo.md](todo.md) 或 [suspended.md](suspended.md)。

## 相关文档

- 当前状态：[../project/status.md](../project/status.md)
- 架构与载荷边界：[../project/architecture.md](../project/architecture.md)、
  [../project/dependencies.md](../project/dependencies.md)
- WebKit DMA-BUF 调查：[webkit-dmabuf-investigation.md](webkit-dmabuf-investigation.md)
- 阶段补丁：[../patches/README.md](../patches/README.md)
- 工具与脚本：[../../tools/README.md](../../tools/README.md)、[../../scripts/README.md](../../scripts/README.md)
