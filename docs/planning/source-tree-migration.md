# 源码树迁移设计（driver source tree migration）

## 状态

**本文件是迁移设计方案，全部内容均为计划，未实施。** 当前设备运行基线仍为
`3.3.3.42-patched-27`（最后一个 patch 模式版本）；迁移期间不改变设备上的运行驱动。

## 一、目标与范围

把"在 Deepin 202504 原包上打补丁"的模型，改为"**仓库内维护驱动源码树 + 清单管理黑盒二进制**"：

1. 内核驱动**可维护源码**（约 7M：innosrvkm PVR 包装 + DPU 显示）完整纳入 git，直接提交迭代，
   不再使用 patch 文件；
2. 黑盒二进制（5 个 `.o_shipped` + 18 个用户态 `.so` + 4 个固件，约 127M）**不进入 git**，
   由 `binary-manifest.json` 清单管理，用幂等提取工具从 pinned Deepin deb 就位；
3. 版本号脱离 Deepin 上游（不再 `-patched-N`），tag = 源码提交 + 清单哈希；
4. 新增四类配套：代码架构分析文档、开发测试闭环、二进制提取工具、用户态组件说明。

## 二、目标目录结构

```text
innogpu-fh2m-debian-trixie/
├── drivers/                        # ★ 内核驱动源码树（git 跟踪，约 7M 源码）
│   ├── innosrvkm/                  #   PVR services 包装（pvr_*.c）+ DPU 显示（innodpu_*/pdp0_*/g3_*）
│   ├── innogpu/ innovpu/ innosmmu/ innodma/ innopmbus/ innopower/
│   ├── include/ Makefile Kbuild dkms.conf modules_config.sh
│   └── （不含任何 .o_shipped —— 由 vendor/ 提供）
├── vendor/                         # ★ 黑盒二进制就位区（gitignored，由提取工具填充）
│   ├── innosrvkm/innosrvkm.o_shipped
│   ├── innogpu/innogpu.o_shipped
│   ├── innovpu/innovpu.o_shipped
│   ├── innosmmu/innosmmu.o_shipped
│   ├── innodma/innodma.o_shipped
│   └── userspace/                  #   用户态 .so 解包区
├── binary-manifest.json            # ★ 二进制清单（唯一权威：来源 deb 路径 + SHA-256 + 角色）
├── scripts/
│   ├── extract-vendor-binaries.sh  # ★ 幂等提取工具（新）
│   ├── build-innogpu-driver.sh     #   从 drivers/ + vendor/ 构建（替代 patch 应用步骤）
│   ├── run-dev-tests.sh            # ★ 开发测试闭环门槛（新）
│   └── check-*.sh / verify-*.sh    #   既有检查与验收入口（保留）
├── tests/
│   ├── package/                    #   既有包边界 fixture
│   ├── kernel/                     # ★ 内核离线编译 + 探针回归测试（新组织）
│   └── README.md
├── docs/
│   ├── project/driver-architecture.md    # ★ 代码架构分析（新）
│   ├── user/userspace-components.md      # ★ 用户态组件说明（新）
│   └── planning/source-tree-migration.md #   本文件
├── debs/                           #   仅 pinned Deepin 原包（唯一二进制来源）
└── patches/                        #   → 退役；内容已转为 drivers/ 上的提交
```

## 三、迁移阶段

### 阶段 0（当前）：设计定稿
- 本文件 + 以下四项专项设计；冻结 `patched-27` 为最后 patch 版本。

### 阶段 1：源码树导入与补丁转提交（试点）
1. 将 Deepin DKMS 源码导入 `drivers/`（仅源码文件；`.o_shipped` 剔除）；
2. 把现有 13 个补丁按内容转成对 `drivers/` 的 13 个提交（commit message 保留
   `patch-0XX` 溯源，body 引用对应 `docs/patches/` 文档）；
3. 编写 `binary-manifest.json` 与 `extract-vendor-binaries.sh`，`vendor/` 就位；
4. 改构建脚本为"仓库树 + vendor/"，去掉 patch 应用；**功能回归必须与 p27 行为一致**
   （探针/桌面/验收全过，见"五、验证门槛"）。

### 阶段 2：版本独立与 CI
- 版本号改为独立迭代（`1.0.0-iN`），tag = 源码提交 + 清单哈希；
- `run-dev-tests.sh` 作为每个提交的门槛（静态 + 离线编译 + fixture）。

### 阶段 3：持续迭代
- 在 `drivers/` 内做真正的重构（invisible READ 预取、DPU 模块化等）；
- 用 `driver-architecture.md` 指导每次改动归属与边界。

---

## 四、四项专项设计

### 4.1 代码架构分析文档（`docs/project/driver-architecture.md`）

用途：指导未来代码更新迭代的"地图"。内容大纲：

1. **总体分层**：ioctl/DRM → GEM/PRIME → DPU 显示 ↔ services 包装 → 固件；用户态 →
   services/GL/VK/OCL/codec；
2. **模块边界表**：每个目录/文件的职责、源码 vs 二进制、维护入口（引用
   [ddk-v119-mapping.md](ddk-v119-mapping.md) 的组件映射）；
3. **关键子系统**：
   - DPU 显示：CRTC/plane/connector 与 vblank/热插拔/背光（全部源码）；
   - GEM/PRIME：visible/invisible VRAM、GTT、DMA-BUF 导入导出、fence（patch-023/025/027 落点）；
   - services 包装：`pvr_*.c` 与预编译核心的调用边界（哪些可改、哪些只读）；
   - 二进制接口契约：PDP ioctl 号、CPU_PREP/CPU_FINI 语义、`innodma` 的 SYS2GDDR/GDDR2SYS；
4. **数据流图**：一次 glClear → 用户态 → GBM → DRM → GEM → DPU/GPU 的完整路径；
5. **修改规则**：新功能应落在哪个模块、不得触碰哪些二进制接口、需要什么探针验证。

### 4.2 开发测试闭环（`scripts/run-dev-tests.sh` + `tests/kernel/`）

目标：每个提交可自动验证，形成"改代码 → 测试 → 合入"闭环。

| 层级 | 工具 | 触发 |
| --- | --- | --- |
| 静态 | `check-docs.sh`、`bash -n`、`git diff --check`、探针编译 | 每次提交 |
| 离线内核 | 对 `drivers/` + `vendor/` 做 `check-deb-dkms-build` 式编译（vermagic 匹配） | 每次提交 |
| fixture | `tests/package/run-boundary-tests.sh` | 每次提交 |
| 实机（设备） | `verify-install-status`、`check-desktop-hwgl`、PDP/vblank/VA-API 探针、能力普查 | 每个候选包 |

`run-dev-tests.sh` 统一执行前三层（无需设备），输出 PASS/FAIL 摘要；
实机层为操作者执行的清单（`tests/kernel/README.md` 记录各探针的判据与期望值）。

### 4.3 二进制清单与幂等提取工具

#### `binary-manifest.json` 格式（草案）

```json
{
  "format_version": 1,
  "source_deb": "debs/innogpu-fh2m_20250421190503-debug_amd64.deb",
  "source_deb_sha256": "<deb 的 SHA-256>",
  "binaries": [
    {
      "path": "innosrvkm/innosrvkm.o_shipped",
      "in_deb": "usr/src/innogpu-kernel-2.2/innosrvkm/innosrvkm.o_shipped",
      "sha256": "<文件 SHA-256>",
      "role": "services core (预编译，只读)"
    },
    {
      "path": "innogpu/innogpu.o_shipped",
      "in_deb": "usr/src/innogpu-kernel-2.2/innogpu/innogpu.o_shipped",
      "sha256": "...",
      "role": "FH2M HAL (预编译，只读)"
    },
    {
      "path": "userspace/innogpu-fh2m/libVK_INNO.so",
      "in_deb": "usr/lib/x86_64-linux-gnu/innogpu-fh2m/libVK_INNO.so",
      "sha256": "...",
      "role": "Vulkan ICD (预编译，只读)"
    }
    // ... innovpu/innosmmu/innodma .o_shipped、其余 17 个 .so、4 个固件
  ]
}
```

#### `scripts/extract-vendor-binaries.sh`（幂等）

- 输入：`binary-manifest.json`（默认）+ 可选 `--check-only`；
- 行为（对每个清单条目）：
  1. 目标 `vendor/<path>` 已存在且 SHA-256 匹配 → **跳过**（幂等，无操作）；
  2. 缺失或哈希不匹配 → 从 `debs/` 的 pinned deb 解出并校验哈希后写入；
  3. 源 deb 缺失/哈希不符 → 报错并说明获取方式；
- `--check-only`：只报告每个条目"就位/缺失/哈希不符"，不做任何写入；
- 设计目标：**克隆后执行一次即就位；之后构建/测试只做 `--check-only` 快速核验**，
  与用户"存放以后不用反复操作，最多检查是否修改"的要求一致。

### 4.4 用户态组件说明（`docs/user/userspace-components.md`）

用途：说明"用户态是什么、干什么用、为什么是二进制、能/不能对它做什么"。

| 组件（18 个 .so 分组） | 角色 | 来源与边界 |
| --- | --- | --- |
| `libsrv_um_inno.so`/`libusc_inno.so`/`libufwriter_inno.so` | services UMD / USC 着色器编译器 / UF writer（Imagination DDK 用户态） | Deepin 原包；预编译；只读 |
| `libVK_INNO.so`（Vulkan 1.3.264）/`libINNOOCL.so`（OpenCL 3.0） | Vulkan / OpenCL ICD | 同上；能力面见 capability-survey.md |
| `libGL_INNO_MESA.so`/`libGLESv2_INNO_MESA.so`/`libGLX_inno.so`/`libglapi_inno.so`/`libinno_mesa_wsi.so` | Mesa 派生 GL/EGL/GLX 栈 | 同源 ABI 整体部署，禁止单文件替换 |
| `libinnogpu_gbm.so`/`libinno_dri_support.so`/`inno_dri.so`/`innogpu_drv.so` | GBM / DRI / Xorg DDX | 同源；DDX 在 `/usr/lib/xorg` |
| `libinno_codec.so`/`innogpu_drv_video.so` | 视频编解码（VA-API 解码 + 私有 codec） | 预编译；编码接口私有，未实机验证 |
| `libifbc.so`/`libifbc_ext.so` | 帧缓冲压缩（IFBC） | 预编译 |
| 固件 `fh2m.fw/fh2m.sh/fh2c.fw/fh2c.sh` | META 微码 + USC shader 固件 | 必须整体保留（事故记录） |

规则：**用户态是"原子载荷"，随 Deepin 基线整体升级，不拆不混**；本项目对它的"修改"只能是
能力面验证（探针）、调用画像（trace-loader）和文档记录，不能改 .so 内部。

## 五、验证门槛（阶段 1 完成判据）

1. `drivers/` 导入完整、13 个补丁转提交后与 p27 源码逐字一致（diff 为空）；
2. `extract-vendor-binaries.sh` 首次执行就位 5+18+4 项、二次执行全跳过、`--check-only` 全 PASS；
3. 新构建流程离线 DKMS 编译通过、vermagic 匹配；
4. 功能回归与 p27 一致：`verify-install-status`、`check-desktop-hwgl`、PDP READ/WRITE 探针、
   vblank 探针、VA-API 探针、能力普查摘要；
5. `run-dev-tests.sh` 全绿；`check-docs.sh` 通过。

## 六、回退与风险

- 迁移期间设备保持 p27；阶段 1 产物先离线验证，通过后才允许安装；
- 回退 = 装回 p27 deb（debs/ 保留）或签出迁移前的 main 提交；
- 风险与对策：
  - 源码树与 Deepin 上游脱节 → manifest 保留来源引用，升级基线=重新导入 diff；
  - git 膨胀 → 二进制不进 git（127M 全部走 manifest + vendor/）；
  - 补丁溯源丢失 → 每个转提交保留 `patch-0XX` 标记与文档链接；
  - 许可证 → 保留文件头 Dual MIT/GPLv2 声明。

## 参考

- [ddk-v119-mapping.md](ddk-v119-mapping.md)：组件谱系与二进制边界。
- [release-review-2026-08-20.md](release-review-2026-08-20.md)：可复现构建纪律。
- 现有 `scripts/check-deb-dkms-build.sh` 的离线编译逻辑作为新构建的基础。
