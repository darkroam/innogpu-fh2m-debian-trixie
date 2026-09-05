# fantgpu 基座更新迭代评估（R16）

- **署名**：qoder
- **日期**：2026-09-04
- **状态**：P0 重新放行（新口径 198/175/2，dsh 终审 2026-09-04）；P1–P3c 维持放行（不依赖
  版权人计数）；P3c 提交 `f8e9979`。**P4 已放行**（dsh 终审 2026-09-04）。**P5 当前阶段**
  （A/B 路线表，初审/终审流程中；P5 尚未放行）。P6 及其后未授权。

## 概述

本文档为 R16 基座更新迭代评估的权威记录，涵盖 19 项台账的 D/O/F 判定矩阵及 P0–P3c 结论。

- **D**：Deepin 原始基座（`build/r16-unpack-D/`）
- **O**：当前有效 i3 staging（`drivers/innogpu/`）
- **F**：fantgpu 新候选（`build/r16-fantgpu-deb/`）

## 19 项台账 D/O/F 矩阵

### P3a 台账（9 项，dsh 终审已放行）

| # | Patch | 描述 | 语义项 | F 已覆盖 | 处置 |
|---|-------|------|--------|---------|------|
| 1 | 001 | kernel 6.12 compat | 18 | 2（1 better + 1 partial） | adapted-port |
| 2 | 002 | DP fbdev fallback | 2 | 0 | adapted-port |
| 3 | 006 | connector ACPI map | 4 | 4（3 sem-eq + 1 partial） | adapted-port |
| 4 | 007 | fbdev IO mmap | 1 | 0 | adapted-port |
| 5 | 009 | internal eDP connector | 4 | 0 | adapted-port |
| 6 | 023 | invisible read no writeback | 5 | 0 | adapted-port |
| 7 | 025-dma | DMA resv usage R/W | 2 | 0 | adapted-port |
| 8 | 026-vblank | inactive CRTC guard | 1 | 0 | adapted-port |
| 9 | 027 | foreign dmabuf lifecycle | 4 | 1（sem-eq） | adapted-port |

**P3a 统计**：41 语义判定项，已覆盖 7（1 better + 4 sem-eq + 2 partial），需处理 34，0 可 drop。

**关键发现**：
- 001 是编译前提：F 缺 16/18 语义项，当前无法在 Trixie 内核编译
- 006 的 2880x1800 刷新率 F=90Hz vs 补丁=60Hz，需确认
- 027 prime_import 类型混淆是潜在崩溃风险
- 023 存在无条件回写路径，运行时性能影响需实测

### P3b 台账（4 项，dsh 终审已放行）

| # | Patch | 描述 | 语义项 | F 已覆盖 | 处置 |
|---|-------|------|--------|---------|------|
| 10 | 024 | DVFS power-state guard | 1 | 0 | adapted-port |
| 11 | 026-lifecycle | DVFS suspend/resume 协调 | 3 | 0 | adapted-port |
| 12 | 028 | 温度监控延迟重启 | 4 | 0 | adapted-port |
| 13 | 029 | DDCCI 面板背光处理 | 2 | 0 | adapted-port |

**P3b 统计**：10 语义判定项，已覆盖 0，需处理 10，0 可 drop。

**关键发现**：
- 024 + 026-lifecycle 构成完整 DVFS 协调，F 完全缺失
- 028 基础设施存在但未连接（原子计数协调缺失）
- 029 DDCCI F 通过隐式排除实现部分效果，但缺少显式逻辑
- 024 的 `PVRSRVDefaultDomainPower` 开源源码无实现但 shipped object 有

### P3c 台账（6 项，dsh 终审已放行）

| # | Patch | 描述 | 一级分类 | 二级处置 |
|---|-------|------|---------|---------|
| 14 | stage-000 | 初始基线 | absent + unassessable | runtime-verify |
| 15 | 003 | 关闭项 | absent | retain |
| 16 | 004 | 关闭项 | absent | retain |
| 17 | 005 | 关闭项 | absent | retain |
| 18 | 008 | 关闭项 | absent | retain |
| 19 | 025-display | 关闭项 | absent | runtime-verify |

**P3c 统计**：6 个 patch，8 个语义判定项。语义项口径：0 项静态证实已覆盖，6 项 retain（003×2 + 004×2 + 005×1 + 008×1），2 项 runtime-verify（stage-000×1 + 025-display×1），0 项 adapted-port，0 项 drop。patch 口径：4 retain + 2 runtime-verify。

## 汇总

| 检查点 | 语义项总数 | 已覆盖 | retain | runtime-verify | 需处理（adapted-port） | 可 drop |
|--------|-----------|--------|--------|---------------|----------------------|---------|
| P3a | 41 | 7 | 0 | 0 | 34 | 0 |
| P3b | 10 | 0 | 0 | 0 | 10 | 0 |
| P3c | 8 | 0 | 6 | 2 | 0 | 0 |
| **总计** | **59** | **7** | **6** | **2** | **44** | **0** |

校验：7 + 6 + 2 + 44 = 59 ✓

## P0–P3c 结论

### P0：身份与许可预检 ✓

- D/O/F 三源身份确认
- 许可预检通过
- 新旧判定完成

### P1：解包与载荷 ✓

- deb 源码树核对完成
- data typed manifests：678/660/61
- control manifests：5/5
- 固件/hwinfo 检索完成

### P2：枚举/API 差异索引 ✓

- 463 文件对，432 differs / 31 identical
- 467 行 normalized manifest（463 对 + 3 F-only，SHA-256 `2eb02ab1…`）
- 重命名规则确定（normalization script）
- enum/struct/API/ABI 事实索引完成

### P3a：9 项有效源码修改 ✓

- 41 语义判定项逐项 D/O/F 判定
- 7 项已覆盖，34 项需 adapted-port
- 0 项可 drop
- codex 初审 → 四修 → dsh 终审放行

### P3b：suspend 4 项 ✓

- 10 语义判定项逐项 D/O/F 判定
- 0 项已覆盖，10 项需 adapted-port
- 0 项可 drop
- codex 初审 → 二修 → codex 复审放行 → **dsh 终审放行**（`report.md` 节「dsh 终审（P3b ·
  2026-09-04）：放行，仅进入 P3c」，其中 dsh 独立抽验 4 个代码点并认可「4 项 suspend patch
  全部 absent/adapted-port，10/10 语义项需移植，0 可 drop」）

### P3c：stage-000 + 关闭/未验证 5 项 ✓

- 6 个 patch，8 个语义判定项
- 0 项静态证实已覆盖
- 6 项 retain（003×2 + 004×2 + 005×1 + 008×1：所检查局部语义与 O 一致，保持关闭）
- 2 项 runtime-verify（stage-000×1：调用链仍存在，崩溃是否修复 unassessable；025-display×1：根因假设未证实/证伪）
- 0 项 adapted-port，0 项 drop
- 003/004/005 的回归验证登记为 P4/P5 测试缺口
- codex 四次复审 → dsh 终审放行（提交 `f8e9979`）

### P4：整栈可分性评估（codex 初审返工中）

#### P4-1：Shipped object 耦合分析

5 个 `.o_shipped` 闭源二进制 blob，D 与 F 各自一套。

| 子系统 | D 大小 | F 大小 | D undef | F undef | D def | F def |
|--------|--------|--------|---------|---------|-------|-------|
| GPU 核心 | 6.4 MB | 6.6 MB | 202 | 224 | 1,521 | 1,571 |
| PVR srvkm | 47.6 MB | 47.9 MB | 725 | 736 | 3,645 | 3,660 |
| VPU | 3.3 MB | 3.3 MB | 109 | 110 | 268 | 269 |
| DMA | 569 KB | 577 KB | 137 | 139 | 187 | 192 |
| SMMU | 531 KB | 537 KB | 122 | 123 | 206 | 206 |

**跨 blob 依赖（nm 实测）**：

| 依赖方向 | D | F |
|----------|---|---|
| srvkm → GPU | 57 | 60 |
| VPU → GPU | 13 | 13 |
| DMA → GPU | 26 | 26 |
| SMMU → GPU | 18 | 18 |
| srvkm → DMA | 4 | 4 |
| VPU → DMA | 4 | 4 |
| GPU → DMA（F only） | 0 | 1 |
| **跨 blob 总计** | **122** | **126** |

依赖结构：GPU blob 为主要枢纽（4 个外围 blob 均依赖 GPU 的 `fh2m_hal_*` 函数），
DMA 为次级节点（srvkm 和 VPU 依赖 DMA 的 memcpy 辅助函数）。F 中 GPU→DMA 有 1 个
符号依赖，构成 GPU↔DMA 双向环。D 中 GPU→DMA 为 0，为单向。外围 blob 之间：srvkm↔vpu、
srvkm↔smmu、vpu↔smmu 均为 0。

**关键纠正**：
- 原报告称"~1144 个未解析符号"为源码层 `INNO_EXT_SYM` 宏调用行数，**不是**实际
  undefined symbol 数。nm 实测 D 总 undefined = 1295，F 总 undefined = 1332。
- 跨 blob 依赖仅占每个 blob undefined 的 0-19%；其余 81-100% 为外部符号（wrapper 或内核）。
- 已观测到以 GPU 为主要枢纽、DMA 为次级节点的稀疏符号依赖图；耦合强度及跨树 ABI
  兼容性 unassessable（未覆盖共享结构布局、回调和版本 ABI）。
- 跨树混装（D blob + F wrapper 或反之）因 `inno_*` / `fant_*` 前缀差异
  unsupported/unassessable，默认采用同代完整 coherent set。
- P3c stage-000 为 runtime-verify；路径 A 的 binary workaround 需复现后评估，不预设为
  "必需新二进制补丁"。

**结论**：同树 5 blob 的符号依赖图以 GPU 为主要枢纽、DMA 为次级节点，耦合强度
unassessable。跨树混装 unsupported/unassessable，默认采用同代完整 coherent set。
路径 A 接受 F coherent set，路径 B 保留 D coherent set。

#### P4-2：包共存与用户态隔离

**包级别**：D（`innogpu-fh2m`）与 F（`fantgpu-fh2m`）**不可共存**。

| 维度 | 证据 |
|------|------|
| F Replaces | `innogpu-fh2m` 在列表中（`DEBIAN/control:3`） |
| F Conflicts | `innogpu-fh2m` 在列表中（`DEBIAN/control:4`） |
| 文件冲突 | `/usr/lib/kgc/kgc_inno.so`：D SHA-256 `6504bcd0…fbaa5f`（14616 B），F SHA-256 `835aaa29…6795`（10456 B）——同路径不同内容 |
| Xorg 二进制 | 两包 postinst 均修改 `/usr/bin/Xorg`（D 替换 `inno_basedir`，F 替换 `fant_basedir`） |
| initramfs | 两包均执行 `update-initramfs -u -k all` |
| ldconfig | 两包均删除 `/etc/ld.so.cache` 并重建 |
| Kylin 模块路径 | 两包均操作 `/lib/modules/*/kernel/kylin/gpu/inno/` |

**全局状态修改矩阵**（maintainer scripts）：

| 全局状态 | D | F | 冲突 |
|----------|---|---|------|
| `/etc/modules` | 追加 `i2c-algo-bit` | 追加 `i2c-algo-bit` | 幂等（先检查） |
| `/usr/bin/Xorg` | 二进制补丁 `inno_` | 二进制补丁 `fant_` | **同文件** |
| `/etc/systemd/system/` | `sw-inno-gl.service` | `sw-fant-gl.service` | 不同名 |
| ALSA UCM | `InnosiliconCard*` | `FantasyCard*` | 不同名 |
| `/etc/.fantgpu.cfg` | 不使用 | 写入设备配置 | F-only |
| DKMS | `innogpu-kernel` 2.2 | `fantgpu-fh2m-kernel` 2.2 | 不同模块名 |
| modprobe blacklist | 临时：`blacklist-innogpu.conf`（postinst 创建/删除） | 持久：`blacklist-fh2m.conf`（黑名单 D 模块）；临时：`blacklist-fantgpu.conf`（postinst 创建/删除） | F 持久黑名单 D 模块 |

**用户态命名隔离**（loader 级别）：

| ABI 边界 | O | F |
|----------|---|---|
| DRM 驱动名 | `"innogpu"` | `"fh2m"` |
| PVR DRM 名 | `"inno"`（`config_kernel.h:166`） | `"ft"`（`config_kernel.h:166`） |
| DRI .so | `innogpu_dri.so` | `fh2m_dri.so` |
| VA-API .so | `innogpu_drv_video.so` | `fh2m_drv_video.so` |
| Vulkan ICD | `innoconf.json` → `libVK_INNO.so` | `fh2m_conf.json` → `libVK_FANT_fh2m.so` |
| EGL vendor | `00_inno.json` → `libEGL_inno.so.0` | `00_fh2m.json` → `libEGL_fh2m.so.0` |
| OpenCL ICD | `INNO.icd` → `libINNOOCL.so` | `FANT_fh2m.icd` → `libFTOCL_fh2m.so` |
| Xorg DDX | `innogpu_drv.so` / `"innogpu"` | `"fh2m"` |
| driconf | `kernel_driver="innogpu"` | `kernel_driver="fh2m"` |
| 内核模块名 | `innogpu` | `fantgpu` |

**结论**：两包 dpkg 级别禁止共存（Conflicts/Replaces），存在实际文件冲突
（`kgc_inno.so`），且均修改 `/usr/bin/Xorg` 等全局状态。用户态 loader 名称不同，
但必须**协调的整栈替换，不允许混合稳态**——安装 F 前必须卸载 D，回退同理。备份、
中断恢复、卸载/安装顺序和回退验证留给 P5 矩阵。

#### P4-3：固件

**GPU 固件载荷**（两包均有 4 个文件）：

| 文件 | D 路径 | D SHA-256 | F 路径 | F SHA-256 | 内容 |
|------|--------|-----------|--------|-----------|------|
| fh2m.fw | `lib/firmware/innogpu/fh2m.fw` | `c406f46b…` | `lib/firmware/fantgpu/fh2m/fh2m.fw` | `8d39a405…` | **不同** |
| fh2m.sh | `lib/firmware/innogpu/fh2m.sh` | `f2b0ead7…` | `lib/firmware/fantgpu/fh2m/fh2m.sh` | `f2b0ead7…` | **相同** |
| fh2c.fw | `lib/firmware/innogpu/fh2c.fw` | `abffcb90…` | `lib/firmware/fantgpu/fh2m/fh2c.fw` | `96043630…` | **不同** |
| fh2c.sh | `lib/firmware/innogpu/fh2c.sh` | `f2b0ead7…` | `lib/firmware/fantgpu/fh2m/fh2c.sh` | `f2b0ead7…` | **相同** |

**hwinfo_g0m.bin**：**两包均不含**。D 日志中的加载请求失败是文件不存在的证据，
不是文件存在的证据。hwinfo_g0m.bin 来源 unassessable。

**结论**：固件拆为两个维度——
1. GPU 固件载荷：两包均有 4 文件，安装路径不同（`innogpu/` vs `fantgpu/fh2m/`），
   `.fw` 内容不同，`.sh` 内容相同。路径 A 采用 F 固件并验证；路径 B 保留 D 固件。
2. hwinfo_g0m.bin：两包均缺失，来源 unassessable，与基座选择无关。

#### P4-4：许可与 policy 成本（五层分解）

**P0 许可预检边界**：P0 仅覆盖 F 466 个 .c/.h 源码文件的版权声明分类，**不覆盖**
shipped blob、固件、用户态闭源载荷。预检不是完整授权结论。

**F 源码分类（466 个 .c/.h）**：

| 分类 | 数量 | 说明 |
|------|------|------|
| Dual MIT/GPLv2 | 405 | 主许可 |
| BSD-3-Clause OR LGPL-2.1-only | 2 | fantpmbus |
| Strictly confidential | 3 | 不公开 |
| MIT-only | 4 | 无 GPL 双授权 |
| Unclassified | 52 | 无明确许可声明 |
| **小计** | **466** | |

**版权人分布**（口径：每文件去重 + Copyright 行 + 按精确法定名称分行，不合并别名；
`LC_ALL=C rg -l -g '*.c' -g '*.h' 'Copyright.*HOLDER' ROOT`）：

| 版权人 | D 文件数 | F 文件数 |
|--------|---------|---------|
| Innosilicon Technology Ltd. | 198 | 0 |
| Beijing Fantasy Technology Ltd. | 0 | 175 |
| Fantasy Technology Ltd. | 0 | 2 |
| Imagination Technologies Ltd. | 236 | 237 |
| CHIPS&MEDIA INC. | 2 | 2 |

注：F 中 `Fantasy Technology Ltd.`（无 "Beijing" 前缀）出现在 `fantpmbus/fantpmbus_drv.c`
和 `fantpmbus/fantpmbus_drv.h`。若按 Fantasy 别名合并则为 `177`，但本表按精确法定名称
分行以保持口径一致。

**五层成本分解**：

| 层 | 内容 | 路径 A 成本 | 路径 B 成本 |
|----|------|------------|------------|
| 1. 开源候选 | F .c/.h 中 dual MIT/GPL + BSD/LGPL（407 文件） | 中：重生成 allowlist/inventory/policy 计数 | 逐文件审查来源/许可 + 更新 inventory hash |
| 2. Unclassified/confidential | 52 unclassified + 3 confidential + 4 MIT-only | 高：需逐文件确权或排除 | 同左（每个 port 项须审查） |
| 3. Shipped blob | 5 .o_shipped（~61 MB） | 高：需新授权或 EULA 评估 | 已有授权维持 |
| 4. 固件 | 4 GPU 固件文件 | 中：验证 F 固件兼容性 | 无变更 |
| 5. 用户态闭源 | DRI/Vulkan/EGL/OpenCL/VA-API .so | 高：需新授权或 EULA 评估 | 已有授权维持 |

**其他成本**：
- `driver-source-allowlist.txt` 重生成（路径 A）
- `source-license-inventory.tsv` 重建（路径 A）
- `license-audit-policy.json` expected_summary 更新（路径 A）
- `THIRD_PARTY_NOTICES.md` 版权人名称更新（路径 A）
- `audit-licenses.py` 参数化调整（路径 A）
- `/usr/share/doc/<package>/copyright` 补建（两包均缺失，Debian policy 要求）

**许可结论**：P0 预检确认 F 源码许可结构与 D 一致（Dual MIT/GPLv2 为主），但完整
授权须覆盖 blob/固件/用户态闭源载荷，这些均未取得新授权。路径 B 的每个 selected(F-D)
语义项须逐文件审查来源/许可/版权，非零成本。

#### P4-5：结构演进与可分性

**P2 矩阵校正**：463 文件对中 31 identical、432 differs，另有 3 F-only。
**不是**"全部 differs"。

**路径 A（F 基座）——整栈命名空间一致**：

| 维度 | 不可拆分粒度 | 说明 |
|------|-------------|------|
| 构建/模块名 | 整栈 | `fantgpu.ko`、`fantgpu-fh2m` 包名 |
| DKMS 配置 | 整栈 | `PACKAGE_NAME="fantgpu-fh2m-kernel"` |
| 安装路径 | 整栈 | `/kernel/drivers/gpu/drm/fantgpu/` 或 `/updates` |
| 二进制补丁 | 特定 blob | 需针对 `fantgpu.o_shipped` 的新补丁 |

**路径 B（O 基座）——语义级可分**：

| 维度 | 可分性 | 说明 |
|------|--------|------|
| 31 identical 文件 | 无需 port | D/O/F 三树一致 |
| 432 differs 中的语义项 | 按子系统/文件/hunk 判断 | P3a/P3b 已证明 adapted-port 可行 |
| F-only 文件（3 个） | 逐项评估 | 是否为 O 缺失的功能 |
| 子系统边界 | 可独立 port | gpu/dma/smmu/srvkm/pmbus/power/vpu 各子系统独立 |

**结论**：路径 A 的构建/模块/DKMS/安装命名空间应整栈一致。路径 B 的厂商改进按
子系统、文件、结构 ABI 和语义项判断可分性，不复制 F 前缀。P3a/P3b 的 adapted-port
方法已验证语义级移植可行。

#### P4 汇总

| 维度 | 不可拆分粒度 | 路径 A 影响 | 路径 B 影响 |
|------|-------------|------------|------------|
| Shipped object | 同树 coherent set（跨树 unsupported） | 接受 F coherent set | 保留 D coherent set |
| 包共存 | dpkg 禁止共存 | 协调的整栈替换，不允许混合稳态 | 保留 D，回退同理 |
| 固件 | GPU 固件两包均有；hwinfo 两包均缺 | 采用 F 固件并验证 | 保留 D 固件 |
| 许可/policy | 五层分解，blob/用户态需新授权 | 高工作量 | 逐文件审查（非零） |
| 结构/重命名 | A 整栈一致；B 语义级可分 | 接受 F 命名空间 | 按语义项 port |

### P5：A/B 路线表（qoder 执行中）

**路线定义**（`request.md:38-46`）：
- **路径 A** = F + required(O-D)：接受 F 基座，补回 O 中必需但 F 缺失的修复
- **路径 B** = O + selected(F-D)：保留 O 基座，选择性移植 F 中厂商改进

#### 路径 A：F 基座 + required(O-D)

**补丁清单（19 项台账完整处置）**：

**语义项分类**（P3 口径，每项只计一次）：
- **covered**：7 语义项（F 已覆盖，无需处理）
- **need-process**：44 语义项（需 adapted-port 到 F）
- **retain**：6 语义项（P3c 关闭项，保留验证）
- **runtime-verify**：2 语义项（需运行时验证，**尚未执行**）
- **闭合校验**：7 + 44 + 6 + 2 = 59 ✓

**Patch 级处置明细**：

| # | Patch | 语义项 | F 已覆盖 | 需处理 | Patch 级处置 | 说明 |
|---|-------|--------|---------|--------|-----------|------|
| 1 | 001 | 18 | 2 | 16 | adapted-port 16 项 | F 缺 16/18 个 6.12 兼容语义项，**构建前提** |
| 2 | 002 | 2 | 0 | 2 | adapted-port 2 项 | F 完全缺失 |
| 3 | 006 | 4 | 4 | 0 | **整体 adapted-port** | F 已覆盖（3 sem-eq + 1 partial），但因 partial/刷新率裁决整体不能 drop，需 reconcile |
| 4 | 007 | 1 | 0 | 1 | adapted-port 1 项 | F 完全缺失 |
| 5 | 009 | 4 | 0 | 4 | adapted-port 4 项 | F 完全缺失 |
| 6 | 023 | 5 | 0 | 5 | adapted-port 5 项 | F 完全缺失，运行时性能影响需实测 |
| 7 | 025-dma | 2 | 0 | 2 | adapted-port 2 项 | F 完全缺失 |
| 8 | 026-vblank | 1 | 0 | 1 | adapted-port 1 项 | F 完全缺失 |
| 9 | 027 | 4 | 1 | 3 | adapted-port 3 项 + reconcile 1 项 | F 已覆盖 1 sem-eq，但 prime_import 类型混淆需 reconcile |
| 10 | 024 | 1 | 0 | 1 | adapted-port 1 项 | F 完全缺失，DVFS 协调前提 |
| 11 | 026-lifecycle | 3 | 0 | 3 | adapted-port 3 项 | F 完全缺失，DVFS suspend/resume |
| 12 | 028 | 4 | 0 | 4 | adapted-port 4 项 | F 基础设施存在但未连接 |
| 13 | 029 | 2 | 0 | 2 | adapted-port 2 项 | F 隐式排除部分效果，缺显式逻辑 |
| 14 | stage-000 | 1 | 0 | 0 | **runtime-verify** | 初始基线验证（**尚未执行**） |
| 15 | 003 | 2 | 0 | 0 | **retain** | 关闭项 |
| 16 | 004 | 2 | 0 | 0 | **retain** | 关闭项 |
| 17 | 005 | 1 | 0 | 0 | **retain** | 关闭项 |
| 18 | 008 | 1 | 0 | 0 | **retain** | 关闭项 |
| 19 | 025-display | 1 | 0 | 0 | **runtime-verify** | 关闭项，需运行时验证（**尚未执行**） |

**Patch 级统计**：
- adapted-port：13 patch（含 006 整体 adapted-port + reconcile，027 adapted-port + reconcile）
- retain：4 patch（003/004/005/008）
- runtime-verify：2 patch（stage-000/025-display，**尚未执行**）
- **构建前提**：001 的 16 项 6.12 兼容必须先 port，否则 F 无法在 Trixie 编译

**不可拆分项**：
- 接受 F 5 blob coherent set（`fantgpu.o_shipped` 等）+ F wrapper
- 耦合强度 unassessable（P4 仅验证同树跨 blob 符号边，共享结构/回调/版本 ABI 未评估）
- 跨树混装 unsupported（`inno_*`/`fant_*` 前缀差异）

**许可成本**：
- 五层全量重做：allowlist/inventory/policy/NOTICE
- ~466 .c/.h 文件 + 5 blob + 4 固件 + 用户态闭源载荷
- **需新授权评估**：blob 和用户态闭源载荷当前授权不覆盖 F 命名空间

**版本策略**（待用户决策）：
- F 候选包版本：`3.3.8.126-driver-linux-desktop-sp-generic`
- 候选包名：`fantgpu-fh2m-trixie`（待确认，**control 尚未生成**）
- 项目版本序列：下一版本号（如 `4.0.3` 或 `5.0.0`，待用户决策）
- 下一审核 epoch：待确定
- dpkg 升级排序：**需设计并验证 Conflicts/Replaces/文件接管**（F 原包 Conflicts/Replaces 仅覆盖 `innogpu-fh2m` 等厂商包，不含当前 O 包名 `innogpu-fh2m-trixie`）

**包切换与回退**（高风险）：
- O→F 迁移：卸载 `innogpu-fh2m-trixie` → 安装 `fantgpu-fh2m-trixie`
- 文件冲突：`/usr/lib/kgc/kgc_inno.so`（同路径不同内容）
- 全局状态修改：`/usr/bin/Xorg` 二进制补丁、`/etc/modules`、initramfs、ldconfig、DKMS、modprobe blacklist、用户态载荷（DRI/Vulkan/EGL/OpenCL/VA-API .so）
- **Conflicts/Replaces 设计**：候选包 `fantgpu-fh2m-trixie` 必须显式 Conflicts/Replaces `innogpu-fh2m-trixie`，并验证 apt/dpkg 升降级
- **备份需求**：完整系统快照或至少 `/usr/bin/Xorg`、`/etc/modules`、`/lib/modules/*/kernel/kylin/gpu/`
- **中断恢复**：安装中断需恢复到 O 或 F，不能混合
- **回退**：F→O 需重复整栈替换流程
- **重启后验证**：全栈功能测试（见验证矩阵）

**验证矩阵**（完整验收，同路径 B）：
- 19 项逐项 fixture（P3a/P3b/P3c 全部语义项）
- Debian 6.12 DKMS 构建（**前提：001 的 16 项必须先 port**）
- 模块/vermagic/符号验证
- **双 clean-build 可复现**：同输入、同 epoch、两个全新构建根逐字节一致（引用现有发布门禁）
- 包载荷/许可/安装回退验证
- **安装失败立即停止/回退**：dpkg 错误立即停止，触发回退到 O，不继续
- 全套现有 CI
- 冷启动、内外屏/热插拔、fbdev/TTY
- **DDX/DRI/GBM/EGL** 全功能验证
- Vulkan/OpenCL/VA-API 全功能
- DMA-BUF/vblank
- s2idle 与 R14 等价 D1-D6
- PVR/内核错误零增长（**R14 具体错误判据**：`3900372`/`PowerLock`/`POWERED_OFF`/新 `PVR_K:(Error)` 均无；PVR 八项计数不增；真实 deep entry/exit；人工画面/键鼠/TTY 判据；区分已归档 pre-existing 日志）
- **目标 PCI ID**：`1ec8:9810`（fh2m GPU，README 和真机证据确认）；验证 F/O 候选均实际绑定该 ID
- F 新差异回归用例（2880x1800 刷新率、DDCCI 等）

#### 路径 B：O 基座 + selected(F-D)

**完整比较台账**（23 BC labels = 21 differs + 2 F-only，435 文件聚类）：

**工作量等级定义**:
- L0（≤0.5 工作日）：纯文本重写，机械可批量；无逻辑变更。
- L1（1–3 工作日）：机械重写 + 局部编译调试；少量边界条件。
- L2（1–2 周）：需要源码理解、控制流分析、单元测试设计。
- L3（2–4 周）：跨子系统集成、需要 mocked 或真机 fixture、性能回归。
- L4（>4 周 或需要多轮真机）：结构性演进、闭源边界探索、不确定根因。

**selected/drop/defer 释义**:
- **selected**：纳入 B 路线补丁集，路径 B 实施时直接 port。
- **drop**：当前 O 已覆盖该能力，或 F-D 差异无功能价值，不纳入。
- **defer**：当前证据不足以判定，留待 P4/P5 后执行阶段补足（运行时验证、闭源边界等）。

**许可状态映射**（每文件许可，参考 P0 预检；候选级许可为该聚类下所有文件的整体范围）:
- `MIT OR GPL-2.0-only`：开源候选，可作为 O 增量合并对象；P0 已纳入 license inventory。
- `BSD-3-Clause OR LGPL-2.1-only`：开源候选，需在 NOTICE 中保留 SPDX 声明。
- `Strictly Confidential`：未授权；不进入 O；如必要须先获得 EULA。
- `unclassified`：P0 标记为 unclassified；未明确许可，须先确权或排除。
- `non-source`（blob/固件/用户态）：不属 P0 .c/.h 预检范围，沿用当前授权状态。

**验证入口**:
- `tests/unit/run-p2-normalize-tests.sh`：归一化测试，6/6 PASS。
- `tests/unit/run-license-audit-tests.sh`：许可审计测试，50/50 PASS。
- `tests/unit/run-suspend-resume-tests.sh`：suspend/resume fixture（已放行，029 相关）。
- `tests/unit/run-r16-gate-tests.sh`（**本轮新增**）：r16-gate.py 失败关闭测试，13/13 PASS
  （含正/负向 fixture + 确定性重放 + 432 differs/3 F-only 组成和未知 status 负例；详 finding 2/3 返工说明）。
- `tests/unit/run-fantgpu-compat-tests.sh`（**待新增**，P5/P6 后由执行阶段补建）。
- 真机：R14 6/6 deep 矩阵 + 冷启动/内外屏/fbdev/Vulkan/VA-API/DMA-BUF/s2idle。

**23 BC labels完整判定**（21 differs BC clusters + 2 F-only clusters；详 `collab/.../qoder-notes.md` P5 6-7 轮章节；file:line 引用
详 qoder-notes；本节为精简索引 + per-file 处置数据）

> **修订记录（2026-09-04）**：
> - **6 轮（codex 4 P1 finding 2）**：per-file 归一化 + 模式识别完成；BC-07/BC-13
>   由 drop 改 defer。
> - **7 轮（codex 4 P1 finding 1）**：发现 76 BEHAVIORAL 文件仍因所在 BC 被整簇 drop，
>   不符合"完成逐文件内容审查或将未审项失败关闭为 defer"要求。**改为 per-file
>   fail-closed**：任何 BEHAVIORAL 文件无论 BC 默认处置均为 defer。
>   最终 per-file 处置：**113 PURE_RENAME → drop + 319 BEHAVIORAL → defer +
>   3 F-ONLY → drop = 116 drop + 319 defer + 0 selected = 435 文件**。
>   per-file 门禁 PASS（详下）。

| ID | 候选名 | 簇文件数 | per-file drop/defer/F-only | 处置 | 工作量（drop / defer）|
|----|-------|---------|--------------------------|------|----------------------|
| BC-01 | srvkm/public-ddk-headers（srvkm.h/device.h/osfunc.h/allocmem.h/cache_*/debug_common.h/devicemem_*/di_*/dma_*/dpu_*/pdump*/lock*/htbuffer*/...） | 130 | 56/74/0 | drop（56 PURE）+ defer（74 BEHAVIORAL） | L0 / L3 |
| BC-02 | srvkm/bridge-headers（common_*_bridge.h） | 23 | 13/10/0 | drop（13 PURE）+ defer（10 BEHAVIORAL） | L0 / L3 |
| BC-03 | srvkm/ft-public-headers（ft_*.h） | 39 | 25/14/0 | drop（25 PURE）+ defer（14 BEHAVIORAL） | L0 / L3 |
| BC-04 | srvkm/ftx-public-headers（ftx_*.h） | 35 | 11/24/0 | drop（11 PURE）+ defer（24 BEHAVIORAL） | L0 / L3 |
| BC-05 | srvkm/fant-public-headers（fant_*.h） | 11 | 0/11/0 | **全 defer** | — / L3 |
| BC-06 | srvkm/powervr-km-subdir | 2 | 1/1/0 | drop（1 PURE）+ defer（1 BEHAVIORAL） | L0 / L3 |
| BC-07 | srvkm/pdp-headers（pdp_drm.h, pdp0_*, g0/g1/g1p/g3/gen_*） | 6 | 0/6/0 | **全 defer** | — / L1 |
| BC-08 | srvkm/include-structural-evolution（srvkm.h:79-94, device.h:127, osfunc.h:1584-1597, ftx_fwif_km.h:1771-1788） | 4 | 0/4/0 | **全 defer** | — / L3 |
| BC-09 | srvkm/dpu-display（fantdpu_*.c, dp_debugfs.c, pdp0_*.c, gpu_drm.c, g3_*） | 35 | 1/34/0 | drop（1 PURE）+ defer（34 BEHAVIORAL） | L0 / L2 |
| BC-10 | srvkm/ft-sync-stack（ft_*.c: bridge_k/buffer_sync/counting_timeline/drm/dvfs_device/export_fence/fence/gputrace/platform_drv/procfs/sw_fence/sync_file/sync_ioctl_*） | 15 | 2/13/0 | drop（2 PURE）+ defer（13 BEHAVIORAL） | L0 / L3 |
| BC-11 | srvkm/audio | 3 | 0/3/0 | **全 defer** | — / L3 |
| BC-12 | srvkm/event-misc | 8 | 2/6/0 | drop（2 PURE）+ defer（6 BEHAVIORAL） | L0 / L3 |
| BC-13 | gpu/hal（hal.h:168-170 新增 3 宏, hal_power.c:75 收紧守卫） | 11 | 2/9/0 | drop（2 PURE）+ defer（9 BEHAVIORAL） | L0 / L2 |
| BC-14 | gpu/fant-kernel-helpers（全部 .c/.h） | 57 | 0/57/0 | **全 defer** | — / L2 |
| BC-15 | gpu/top-public | 4 | 0/4/0 | **全 defer** | — / L3 |
| BC-16 | dma | 14 | 0/14/0 | **全 defer** | — / L3 |
| BC-17 | power（fant_input_event.c:17-58 新增 PM notifier） | 14 | 0/14/0 | **全 defer** | — / L3 |
| BC-18 | vpu | 8 | 0/8/0 | **全 defer** | — / L3 |
| BC-19 | smmu | 2 | 0/2/0 | **全 defer** | — / L3 |
| BC-20 | pmbus | 2 | 0/2/0 | **全 defer** | — / L3 |
| BC-21 | tools/gpu-info（kgc_gpu_info.c:23 新 include） | 9 | 0/9/0 | **全 defer** | — / L3 |
| BC-22a | F-only: gpu/fant_stackprotector.{c,h} | 2 | 2/0/2 (F-only) | drop | L0 |
| BC-22b | F-only: srvkm/include/common_ri_bridge.h | 1 | 1/0/1 (F-only) | drop | L0 |

**总计**: 23 BC labels = 432 differs + 3 F-only = 435 文件 ✓

**per-file 处置汇总**（详 `build/r16-evidence/per-file-classification.tsv`，
`tools/r16-classify.py` 输出；`tools/r16-gate.py` 验证）：

| 处置 | 文件数 | 说明 |
|------|--------|------|
| **drop** | **116** | 113 PURE_RENAME（fnorm 后 D 内容完全一致）+ 3 F-ONLY（F 端新增模块，per-BC notes 处理）|
| **defer** | **319** | 319 BEHAVIORAL（fnorm 后内容不一致，逐文件 fail-closed）|
| **selected** | 0 | 无 |
| 总计 | 435 ✓ | |

**per-BC 处置表**（详 `build/r16-evidence/per-bc-summary.tsv`）：

| BC | drop | defer | total |
|----|------|-------|-------|
| BC-01 | 56 | 74 | 130 |
| BC-02 | 13 | 10 | 23 |
| BC-03 | 25 | 14 | 39 |
| BC-04 | 11 | 24 | 35 |
| BC-05 | 0 | 11 | 11 |
| BC-06 | 1 | 1 | 2 |
| BC-07 | 0 | 6 | 6 |
| BC-08 | 0 | 4 | 4 |
| BC-09 | 1 | 34 | 35 |
| BC-10 | 2 | 13 | 15 |
| BC-11 | 0 | 3 | 3 |
| BC-12 | 2 | 6 | 8 |
| BC-13 | 2 | 9 | 11 |
| BC-14 | 0 | 57 | 57 |
| BC-15 | 0 | 4 | 4 |
| BC-16 | 0 | 14 | 14 |
| BC-17 | 0 | 14 | 14 |
| BC-18 | 0 | 8 | 8 |
| BC-19 | 0 | 2 | 2 |
| BC-20 | 0 | 2 | 2 |
| BC-21 | 0 | 9 | 9 |
| BC-22a | 2 | 0 | 2 |
| BC-22b | 1 | 0 | 1 |
| **总计** | **116** | **319** | **435** |

**per-BC license 字段明细**（per-BC 审计，codex 第 10 轮 finding 3 整改：
21 BC 详细许可分布；直接自 `awk` 聚合
`build/r16-evidence/per-file-classification.tsv` 复现，表中 432 个 differs 行按 `license_d`
聚合；BC-22a/BC-22b 的 F-only 行单列，另以 `license_f` 核对 F 侧许可，不将两列误称为
全量一致）：

| BC | 簇文件数 | mit-or-gpl-2.0-only | unclassified | strictly-confidential | bsd-3-clause-or-lgpl-2.1-only | mit-only | F-ONLY |
|----|---------|---------------------|--------------|----------------------|--------------------------------|----------|--------|
| BC-01 | 130 | 118 | 12 | 0 | 0 | 0 | 0 |
| BC-02 | 23  | 23 | 0 | 0 | 0 | 0 | 0 |
| BC-03 | 39  | 38 | 0 | 1 | 0 | 0 | 0 |
| BC-04 | 35  | 33 | 1 | 1 | 0 | 0 | 0 |
| BC-05 | 11  | 11 | 0 | 0 | 0 | 0 | 0 |
| BC-06 | 2   | 1 | 0 | 0 | 0 | 1 | 0 |
| BC-07 | 6   | 4 | 1 | 1 | 0 | 0 | 0 |
| BC-08 | 4   | 4 | 0 | 0 | 0 | 0 | 0 |
| BC-09 | 35  | 30 | 5 | 0 | 0 | 0 | 0 |
| BC-10 | 15  | 14 | 1 | 0 | 0 | 0 | 0 |
| BC-11 | 3   | 2 | 1 | 0 | 0 | 0 | 0 |
| BC-12 | 8   | 8 | 0 | 0 | 0 | 0 | 0 |
| BC-13 | 11  | 8 | 3 | 0 | 0 | 0 | 0 |
| BC-14 | 57  | 54 | 3 | 0 | 0 | 0 | 0 |
| BC-15 | 4   | 4 | 0 | 0 | 0 | 0 | 0 |
| BC-16 | 14  | 14 | 0 | 0 | 0 | 0 | 0 |
| BC-17 | 14  | 0 | 14 | 0 | 0 | 0 | 0 |
| BC-18 | 8   | 8 | 0 | 0 | 0 | 0 | 0 |
| BC-19 | 2   | 2 | 0 | 0 | 0 | 0 | 0 |
| BC-20 | 2   | 0 | 0 | 0 | 2 | 0 | 0 |
| BC-21 | 9   | 4 | 5 | 0 | 0 | 0 | 0 |
| BC-22a | 2  | 0 | 0 | 0 | 0 | 0 | 2 |
| BC-22b | 1  | 0 | 0 | 0 | 0 | 0 | 1 |
| **总计** | **435** | **380** | **46** | **3** | **2** | **1** | **3** |

**per-BC 处置口径复核**（codex 第 10 轮 finding 3 关键更正）

- **BC-01** 74 defer 实配 **67 mit-or-gpl + 7 unclassified**（非 `62 + 12`）：74 个
  均为 BEHAVIORAL；全簇 130 个文件另有 56 个 PURE_RENAME/drop，以及 5 个
  unclassified 的 PURE_RENAME/drop。`pdp0_common.h` 属 BC-07，不计入 BC-01。
- **BC-07** 6 文件**全部 BEHAVIORAL/defer**（非"3 PURE + 3 BEHAV"），无
  PURE_RENAME；1 strictly-confidential（`pdp_drm.h`）+ 1 unclassified
  （`pdp0_common.h`）+ 4 mit-or-gpl。
- **BC-11** 仅 3 路径（`srvkm/audio_chip_common.c`/`srvkm/audio_drv.c`/
  `srvkm/audio_print.c`），非 6 路径；2 mit-or-gpl + 1 unclassified
  （`audio_chip_common.c`）。原文误列 `dpu_audio_api.c`/
  `dpu_common_drm_panel.c`/`dpu_compatibility.c` 不在 BC-11 集。
- **BC-16** 14 defer **全部 mit-or-gpl-2.0-only**（非"1 bsd-3-clause-or-lgpl-
  2.1-only (pmbus/pmbus_drv.c/.h)"），PMBus BSD-LGPL 在 BC-20 而非 BC-16。
- **BC-17** 14 defer **全部 unclassified**（非"13 unclassified"），覆盖
  power/* 全部 differs 子集；无 mit-or-gpl 路径。
- **BC-20** 2 defer 文件均为 bsd-3-clause-or-lgpl-2.1-only（`pmbus_drv.{c,h}`），
  非混在 BC-16。

**per-file license 字段**（来自 `build/r16-license-precheck/{D,F}-license-manifest.tsv`）：

| License | 文件数 |
|---------|--------|
| mit-or-gpl-2.0-only | 380 |
| unclassified | 46 |
| strictly-confidential | 3 |
| F-ONLY（D-side N/A）| 3 |
| bsd-3-clause-or-lgpl-2.1-only | 2 |
| mit-only | 1 |
| **总计** | **435** ✓ |

**319 defer 文件许可分布**（按 license_d 字段；本节独立列出，因 codex finding 1 指出"defer 候选均为 MIT/GPL"与生成证据直接矛盾）：

| License | defer 文件数 | drop 文件数 | 总计 |
|---------|--------------|--------------|------|
| mit-or-gpl-2.0-only | 276 | 104 | 380 |
| unclassified | 38 | 8 | 46 |
| strictly-confidential | 2 | 1 | 3 |
| bsd-3-clause-or-lgpl-2.1-only | 2 | 0 | 2 |
| mit-only | 1 | 0 | 1 |
| F-ONLY（D-side N/A）| 0 | 3 | 3 |
| **总计** | **319** | **116** | **435** ✓ |

**43 个非 MIT/GPL defer 文件的代表样本**（详 `build/r16-evidence/per-file-classification.tsv`）：

- `srvkm/include/ftxlayer_impl.h` (BC-04) — strictly-confidential（D/F 一致）
- `srvkm/include/pdp_drm.h` (BC-07) — strictly-confidential（D/F 一致）
- `srvkm/include/pdp0_common.h` (BC-07) — unclassified（D/F 一致）
- `srvkm/include/powervr/fant_drm_fourcc.h` (BC-06) — mit-only（D/F 一致）
- `pmbus/pmbus_drv.{c,h}` (BC-20) — bsd-3-clause-or-lgpl-2.1-only
- `power/fant_devfreq_gov.{c,h}` / `power/fant_input_event.{c,h}` /
  `power/fant_gs8601.c` / `power/fant_is6608a.c` / `power/fant_mp2979a.c` /
  `power/fant_mpq8645p.c` / `power/fant_virture_chip.c` /
  `power/fant_xdpe12284.c` / `power/power.{c,h}` / `power/power_hw_info.{c,h}`
  (BC-17, 14 文件) — unclassified
- `srvkm/audio_chip_common.c` (BC-11) — unclassified
- `srvkm/dpu_dp_common.c` / `srvkm/dpu_hdmi_common.c` / `srvkm/dpu_vga_debugfs.c` /
  `srvkm/pdp0_common.c` (BC-09, 4 文件) — unclassified
- `srvkm/ft_platform_drv.c` (BC-10) — unclassified
- `gpu/fant_audio.{c,h}` / `gpu/fant_idr.c` (BC-14, 3 文件) — unclassified
- `gpu/hal_power.c` / `gpu/kernel_compat.c` (BC-13, 2 文件) — unclassified
- `srvkm/include/audio_chip_common.h` / `config_kernel.h` / `dpu_panel_backlight.h` /
  `dpu_panel_pwr.h` / `dpu_vga_common.h` / `dpu_vga_debugfs.h` /
  `version.h` (BC-01, 7 文件) — unclassified
- `tools/gpu_info/{gpu_info_fantml.{c,h},gpu_info_fantml_km.h,kgc_gpu_info.{c,h}}`
  (BC-21, 5 文件) — unclassified

**含义**：43 个 defer 文件（319 中 13.5%）非 MIT/GPL；按授权语义分两类
（codex 9 轮 finding 4 订正）：
- **40 个需确权**：38 unclassified + 2 strictly-confidential —— 当前无明确
  开源许可，确权前不可移植；
- **3 个已具开源许可，需履行 LICENSE/NOTICE**：2 bsd-3-clause-or-lgpl-2.1-only
  + 1 mit-only —— 不应与未授权项混为一类，授权义务为履行对应许可文本与
  NOTICE 归属，不需新授权；
路径 B 当前许可风险等级由"低"上调至"中"。

**defer 候选要点（21 BC / 319 文件，BC-level 语义补丁项；per-file fail-closed 决定
继承）**（file:line 详 qoder-notes；按 BC-01 → BC-21 顺序列出所有 defer 的 21 个 BC；
BC-22a/BC-22b 为 F-only 走 drop）：

每条 BC 补丁项包含：候选名（簇范围）/类型/处置/依赖/与 O 冲突/许可/适配方式/
验证入口/工作量。**BC-level 默认处置**为该 BC 的整体判定（drop/defer）；per-file
fail-closed 把 BC 默认 drop 中的 BEHAVIORAL 文件改判为 defer —— 即"BC drop +
per-file BEHAV defer"。

**语义证据边界（第 11 轮修订）**：`PURE_RENAME` 仅表示该文件经过当前
fnorm 规则后与 D 字节相等，可支持 drop/L0；`BEHAVIORAL` 仅表示 fnorm 后仍有
内容差异，`first_diff_d/first_diff_f` 是可重放的定位证据，不是“存在行为变化”或
“纯重命名”的最终语义证明。因此 319 个 BEHAVIORAL/defer 文件全部保持未裁决，
不得据此直接进入 selected，也不得把所在 BC 写成已证实的 L0/纯重命名。下表把
每个 BC 当前可观察到的证据与尚未完成的语义裁决分开：

| BC | BEHAVIORAL/defer | 当前可审查证据 | 语义裁决边界 |
|----|------------------|----------------|--------------|
| BC-01 | 74 | 公共 header 归一化不一致；BC-08 的字段/签名差异另列 | 74 个文件未证明纯重命名；逐文件 first_diff + header/API 影响复核，L3 |
| BC-02 | 10 | common bridge header 归一化不一致 | 未证明仅命名空间变化；需 bridge ABI/调用方复核，L3 |
| BC-03 | 14 | ft_ 公共 header 归一化不一致 | 未证明仅前缀变化；需 include/宏/ABI 复核，L3 |
| BC-04 | 24 | ftx_ 公共 header 归一化不一致，含 confidential 文件 | 未证明仅前缀变化；需逐文件授权与 ABI 复核，L3 |
| BC-05 | 11 | fant_ 公共 header 归一化不一致 | 未证明仅前缀变化；需 include/宏/ABI 复核，L3 |
| BC-06 | 1 | `powervr/fant_drm_fourcc.h` 归一化不一致 | mit-only 不等于语义等价；需 fourcc/用户态调用方复核，L3 |
| BC-07 | 6 | `INNO_BIT/FANT_BIT`、framebuffer 函数名未匹配，`DRM_INNO_COMMON_INFO` 删除 | 结构/宏差异已定位，但未完成 PDP0 行为裁决，L1 |
| BC-08 | 4 | `ui32KernelCardID` 字段/函数参数、`OSDivide64u64` 等 API 差异已定位 | API/ABI 影响未裁决，L3 |
| BC-09 | 34 | 模块参数、HPD IRQ 守卫等 3 处控制流差异已定位 | 需回写 O 命名空间并验证 HDMI/HPD，L2 |
| BC-10 | 13 | cleanup 谓词、cardId 关联、`NO_HARDWARE` 守卫等 3 处差异已定位 | 需同步 BC-08 并验证同步/PM 路径，L3 |
| BC-11 | 3 | audio 文件归一化不一致，`audio_drv.c` 含 SeewoOS 分支 | 未证明仅重命名；需 HDMI 音频与 s2idle 复核，L3 |
| BC-12 | 6 | event/trace 文件归一化不一致 | 未证明仅重命名；需事件注册、计数和调用方复核，L3 |
| BC-13 | 9 | 3 个 HAL 宏及 `CONFIG_THERMAL` 守卫差异已定位 | 宏引用和 thermal 行为影响未裁决，L2 |
| BC-14 | 57 | kernel floor、`OSDivide64u64`、DMA 守卫/包装器差异已定位 | 仅部分差异已定位；需逐文件 fence/buffer 复核，L2 |
| BC-15 | 4 | GPU 顶层文件归一化不一致，含 gtt 模块参数 | 未证明仅重命名；需 PCI probe/参数调用方复核，L3 |
| BC-16 | 14 | DMA 文件归一化不一致，`s_dma_raw_cnt` 差异已定位 | 未证明仅重命名；需 DMA 引擎/计数语义复核，L3 |
| BC-17 | 14 | PM notifier、4 个状态分支和蓝牙守卫已定位 | 与 O input/suspend 路径冲突风险未裁决，L3 |
| BC-18 | 8 | VPU 文件归一化不一致 | 未证明仅重命名；需 VPU IOCTL/UMD ABI 复核，L3 |
| BC-19 | 2 | SMMU 文件归一化不一致 | 未证明仅重命名；需 probe/调用方复核，L3 |
| BC-20 | 2 | PMBus 文件归一化不一致，BSD/LGPL 许可已核对 | 许可不等于语义等价；需 PMBus 枚举/调用方复核，L3 |
| BC-21 | 9 | `kgc_gpu_info.c:23` include 差异及 raw-test 路径已定位 | 用户态工具行为/API 未裁决，L3 |

上述“已定位”只表示现有 D/F 文本证据足以指向复核位置；只有对应文件分类为
`PURE_RENAME` 时才可作 drop。其余 BEHAVIORAL 文件须在 `first_diff`、控制流/
调用方、ABI/许可和目标验证结果闭合后，才可升级为 selected 或重新分类；当前
结论统一为 defer。

- **BC-01**（74 defer 文件；BC 默认 drop）：srvkm/public-ddk-headers
  （srvkm.h/device.h/osfunc.h/allocmem.h/cache_*/debug_common.h/devicemem_*/
  di_*/dma_*/dpu_*/pdump*/lock_*/htbuffer_*）。**语义**：vendor + DDK 公共 header
  重命名（fh2m_fant_/FANT_*/__FANT*_H__/__INNO*_H__ 模式）。**与 O 冲突**：除
  BC-08 子集（device.h ui32KernelCardID 字段和 srvkm.h PVRSRVCommonDeviceCreate
  新参数）外，其他 BEHAVIORAL 项尚未证明无行为冲突。**许可**：74 个 defer 中
  67 个 mit-or-gpl-2.0-only + 7 个 unclassified；全簇另有 51 个 mit-or-gpl-2.0-only
  + 5 个 unclassified 的 PURE_RENAME/drop，无 strictly-confidential。**适配方式**：
  PURE_RENAME 子集可视为无功能增量；BEHAVIORAL 子集不作纯重命名结论，
  per-file BEHAV 走 fail-closed defer；BC 整体判定 drop。**验证入口**：
  `bash tests/unit/run-r16-gate-tests.sh`（7 gates）+ 逐文件 first_diff/header/API
  复核 + 真机编译无未定义符号。**工作量**：L0（56 drop）/L3（74 defer）。
- **BC-02**（10 defer 文件；BC 默认 drop）：srvkm/bridge-headers
  （common_*_bridge.h 23 文件中的 differs 子集）。**语义**：PVR bridge 通信
  header 重命名。**与 O 冲突**：无（纯重命名）。**许可**：
  mit-or-gpl-2.0-only。**适配方式**：PURE_RENAME 子集可视为无功能增量；BEHAVIORAL 子集不作纯重命名结论，per-file BEHAV 走
  fail-closed defer。**验证入口**：`run-r16-gate-tests.sh` + 真机 bridge IOCTL
  子集。**工作量**：L0（13 drop）/L3（10 defer）。
- **BC-03**（14 defer 文件；BC 默认 drop）：srvkm/ft-public-headers
  （ft_*.h）。**语义**：F 厂商 ft_ 公共 header 重命名。**与 O 冲突**：未发现已定位的行为冲突，但 BEHAVIORAL 子集尚未完成
  include/宏/ABI 复核。**许可**：mit-or-gpl-2.0-only。**适配方式**：PURE_RENAME
  子集可视为无功能增量；BEHAVIORAL 子集不作纯重命名结论，per-file BEHAV 走
  fail-closed defer。**验证入口**：`run-r16-gate-tests.sh` + 真机编译无未定义符号。
  **工作量**：L0（25 drop）/L3（14 defer）。
- **BC-04**（24 defer 文件；BC 默认 drop）：srvkm/ftx-public-headers
  （ftx_*.h 含 ftxlayer_impl.h strictly-confidential）。**语义**：F 厂商 ftx_
  公共 header 重命名；含 1 strictly-confidential（`srvkm/include/ftxlayer_impl.h`
  D/F 一致）。**与 O 冲突**：无。**许可**：24 中 23 mit-or-gpl-2.0-only + 1
  strictly-confidential（确权前不可移植）。**适配方式**：PURE_RENAME 子集可视为无功能增量；BEHAVIORAL 子集不作纯重命名结论，
  per-file BEHAV 走 fail-closed defer。**验证入口**：`run-r16-gate-tests.sh`
  + strictly-confidential 项确权审查。**工作量**：L0（11 drop）/L3（24 defer）。
- **BC-05**（11 defer 文件；BC 默认 drop）：srvkm/fant-public-headers
  （fant_*.h）。**语义**：F 厂商 fant_ 公共 header 重命名。**与 O 冲突**：未发现已定位的行为冲突，但 11 个 BEHAVIORAL 文件尚未完成 include/宏/ABI 复核。
  **许可**：mit-or-gpl-2.0-only。**适配方式**：不作纯重命名结论，per-file BEHAV
  走 fail-closed defer。**验证入口**：`run-r16-gate-tests.sh`。**工作量**：L3。
- **BC-06**（1 defer 文件；BC 默认 drop）：srvkm/powervr-km-subdir
  （`powervr/fant_drm_fourcc.h` mit-only）。**语义**：F 厂商 powervr/ 子目录
  DRM fourcc header 重命名；1 mit-only（D/F 一致）。**与 O 冲突**：未发现已定位的
  fourcc/API 冲突，但 BEHAVIORAL 文件尚未完成调用方复核。**许可**：mit-only
  （履行 LICENSE/NOTICE 即可移植）。**适配方式**：PURE_RENAME 子集可视为无功能
  增量；BEHAVIORAL 子集不作纯重命名结论，per-file BEHAV 走 fail-closed defer。
  **验证入口**：`run-r16-gate-tests.sh` + NOTICE 归属检查。**工作量**：L0（1 drop）/
  L3（1 defer）。
- **BC-07**（6 文件；BC 默认 drop）：**全部 BEHAVIORAL → defer**（codex 第 10 轮
  finding 3 订正：实测 0 PURE_RENAME + 6 BEHAVIORAL，非"3 PURE + 3 BEHAV"）。
  `pdp0_hw.h` 含 `INNO_BIT/FANT_BIT` 宏置换未匹配；`pdp0_common.h` 含
  `pdp0_get_inno_framebuffer/pdp0_get_fant_framebuffer` 函数名置换未匹配；
  `pdp_drm.h` 含 `DRM_INNO_COMMON_INFO` 删除。行为影响尚未裁决，保守 defer
  待逐文件语义与运行时验证。**许可**：1 strictly-confidential（`pdp_drm.h` D/F 一致）
  + 1 unclassified（`pdp0_common.h`）+ 4 mit-or-gpl-2.0-only。**验证入口**：
  `run-r16-gate-tests.sh` + 真机 PDP0 编译/HPD 验证。**工作量**：L1（~1 周
  3 宏条件性 port + 防御性改动）。
- **BC-08**（4 文件；BC 默认 defer）：公共 API 签名变更——`device.h:127` 新增
  `ui32KernelCardID` 字段；`PVRSRVCommonDeviceCreate` 新增
  `IMG_INT32 ui32KernelCardID` 参数；与 O 现有 `pci_drv` 调用方存在签名冲突。
  `osfunc.h:1584-1597` 新增 `OSDivide64u64` 函数（与 BC-14 联动）。
  **依赖**：BC-01（header rename）。**许可**：mit-or-gpl-2.0-only。**适配
  方式**：需改写：适配 O 命名空间后再 port；新参数须确认是否真的需要（O
  现有调用方都不传 cardId）。**验证入口**：`bash tests/unit/run-r16-gate-tests.sh`
  + fantgpu-compat-tests + 真机冷启动/PCI probe。**工作量**：L3（~6-12 周含
  runtime 验证）。
- **BC-09**（34 defer 文件；BC 默认 defer；1 PURE_RENAME drop + 34 BEHAVIORAL defer）：srvkm/dpu-display
  （fantdpu_*.c/dp_debugfs.c/pdp0_*.c/gpu_drm.c/g3_*）。**语义**：2 处行为
  差异——`fantdpu_dp.c` 移除 `dp_mdelay` 模块参数；`fantdpu_drm_drv.c` 新增
  `s_hdmi_skip_scramble`/`s_hdmi_delay` 模块参数；`fantdpu_dp.c:950` HPD IRQ
  早退路径增加 `(FANTDP_HPD_OUT || FANTDP_HPD_IRQ)` 守卫。**依赖**：BC-08
  间接（device.h ui32KernelCardID）。**与 O 冲突**：含 4 unclassified
  （`srvkm/dpu_dp_common.c/dpu_hdmi_common.c/dpu_vga_debugfs.c/pdp0_common.c`）。
  **许可**：mit-or-gpl-2.0-only + 4 unclassified（确权前不可移植）。
  **适配方式**：把 2 处行为差异回写到 D 命名空间（`innodpu_dp.c` /
  `innodpu_drm_drv.c`）；HPD 守卫需验证是否与 O 6.12 中断处理兼容。**验证
  入口**：`run-r16-gate-tests.sh` + fantgpu-compat-tests + 真机冷启动/HDMI
  热插拔。**工作量**：L2（~2-4 周 hal/驱动的防御性增量 + 真机回归）。
- **BC-10**（13 defer 文件；BC 默认 defer → 仅重命名 drop）：srvkm/ft-sync-stack
  （ft_*.c: bridge_k/buffer_sync/counting_timeline/drm/dvfs_device/export_fence/
  fence/gputrace/platform_drv/procfs/sw_fence/sync_file/sync_ioctl_*）。**语义**：
  3 处行为差异——`ft_bridge_k.c:1041` shutdown 谓词追加
  `&& PVRSRVGetNumCleanupItemsQueued() != 0`；`ft_drm.c:1226` 新增 `cardId`
  局部关联 `ui32KernelCardID`；`ft_buffer_sync.c` `if (!nr_pmrs)` 加
  `#if !defined(NO_HARDWARE)` 包裹。**依赖**：BC-08（device.h field）。
  **许可**：mit-or-gpl-2.0-only。**适配方式**：3 处行为差异全部回写到 D
  命名空间；ft_drm.c 的 cardId 关联 `ui32KernelCardID` 字段（见 BC-08）。
  **验证入口**：`run-r16-gate-tests.sh` + fantgpu-compat-tests + 真机
  suspend/resume 反复。**工作量**：L3（~6-12 周含 runtime 验证）。
- **BC-11**（3 defer 文件；BC 默认 drop）：srvkm/audio（实测仅 `srvkm/
  audio_chip_common.c`/`srvkm/audio_drv.c`/`srvkm/audio_print.c`，非 6 路径；
  codex 第 10 轮 finding 3 订正：`dpu_audio_api.c`/`dpu_common_drm_panel.c`/
  `dpu_compatibility.c` 不在 BC-11 集）。**语义**：F 厂商 audio 子系统重命名；
  `srvkm/audio_drv.c` 仍含 SeewoOS 分支，per-file BEHAV 走 fail-closed defer。
  **依赖**：无。**许可**：2 mit-or-gpl-2.0-only + 1 unclassified
  （`srvkm/audio_chip_common.c`，确权前不可移植）。**适配方式**：不作纯重命名
  结论；3 个 BEHAVIORAL 文件须逐文件复核。**验证入口**：
  `run-r16-gate-tests.sh` + 真机 HDMI 音频 + s2idle 多轮。**工作量**：L3（runtime
  验证）。
- **BC-12**（6 defer 文件；BC 默认 drop）：srvkm/event-misc（event.c/
  trace_events.c 中 differs 子集）。**语义**：F 厂商 event 跟踪子集重命名。
  **依赖**：无。**与 O 冲突**：无。**许可**：mit-or-gpl-2.0-only。**适配
  方式**：PURE_RENAME 子集可视为无功能增量；BEHAVIORAL 子集不作纯重命名结论，per-file BEHAV 走 fail-closed defer。
  **验证入口**：`run-r16-gate-tests.sh` + 真机 PVR 事件跟踪 + 8 项计数不增。
  **工作量**：L3（跨 BC fail-closed 182 文件部分）。
- **BC-13**（9 defer 文件；BC 默认 defer）：gpu/hal（hal.h/hal_dma.c/
  hal_dma_errors.h/hal_power.c/hal_interface.h/.c/hal_bmc.h/hal_bind_numa.c/
  kernel_compat.h/.c/ion_lma_heap.c 中 differs 子集）。**语义**：`hal.h:168-170`
  新增 3 宏 `MCUFW_DDR_WARN_BIT_OFFSET/MANUFACTURER_OFFSET/MANUFACTURER_LEN`；
  `hal_power.c:75` `#if CONFIG_THERMAL` → `#ifdef CONFIG_THERMAL` 收紧守卫。
  **依赖**：BC-14（hal.h 引用 fant_math.c 新函数）。**与 O 冲突**：含
  2 unclassified（`gpu/hal_power.c/kernel_compat.c`）。**许可**：
  mit-or-gpl-2.0-only + 2 unclassified。**适配方式**：3 宏若 O 引用则直接
  复制；`hal_power.c` 收紧是纯防御性，可保留为 O 改动。**验证入口**：
  `run-r16-gate-tests.sh` + fantgpu-compat-tests + 真机 thermal zone 验证。
  **工作量**：L0（2 drop）/L2（9 defer，~2-4 周含防御性改动与真机回归）。
- **BC-14**（57 defer 文件；BC 默认 defer；57 BEHAVIORAL defer）：
  gpu/fant-kernel-helpers（fant_audio/cpumask/debug/dma/dma_buf/drm/drm_mode/
  fence/firmware/fs/idr/insn/interrupt/io/kernel_hook/lock/math/misc/mm/mtrr/
  pci/plat_dev/pm_runtime/srvkm/task/timer/uaccess/uuid/waitqueue 全部 .c/.h
  differs 子集）。**语义**：`fant_fence.c:115` kernel version floor 放松
  `4,14,0` → `4,10,0`（O 已 6.12 不适用）；`fant_math.c:69-73` 新增
  `fh2m_fant_div64_u64` 实现 `OSDivide64u64`（仅在 BC-08 接受时需要）；其他
  差异 `INNOGPU_DMA_ASYNC_IS_TX_COMP_PRESENT` 等新守卫、`fh2m_fant_*` 函数包装、
  `_inno_xxx` kprobe 名称未匹配。**依赖**：BC-08（osfunc.h 声明 OSDivide64u64）。
  **与 O 冲突**：含 3 unclassified（`gpu/fant_audio.c/fant_audio.h/fant_idr.c`）。
  **许可**：mit-or-gpl-2.0-only + 3 unclassified。**适配方式**：仅 2 处行为
  差异，1 处不适用、1 处与 BC-08 联动。**验证入口**：
  `run-r16-gate-tests.sh` + fantgpu-compat-tests + 真机 fence/buffer 验证。
  **工作量**：L2（~2-4 周 hal/驱动的防御性增量 + 真机回归）。
- **BC-15**（4 defer 文件；BC 默认 drop）：gpu/top-public（gpu.h/gpu_pci_drv.c/
  .h/gpu_ion.h 中 differs 子集）。**语义**：F 厂商 GPU 顶层公共 header 重命名；
  `gpu/gpu_pci_drv.c` 含 gtt 模块参数，per-file BEHAV 走 fail-closed defer。
  **依赖**：无。**与 O 冲突**：无。**许可**：mit-or-gpl-2.0-only。**适配
  方式**：PURE_RENAME 子集可视为无功能增量；BEHAVIORAL 子集不作纯重命名结论，per-file BEHAV 走 fail-closed defer。**验证
  入口**：`run-r16-gate-tests.sh` + 真机 GPU PCI probe。**工作量**：L3（跨
  BC fail-closed 182 文件部分）。
- **BC-16**（14 defer 文件；BC 默认 drop）：dma（axi_dmac.h/dma.h/dma_debug.c/
  .h/dma_drv.c/fant_axi_dma_drv.c/.h/fant_dmaengine.h/fant_pcie_dma_drv.c/.h/
  pcie_dmac.h/utils.h/virt_dma.c/.h 中 differs 子集）。**语义**：F 厂商 DMA
  子系统重命名；`dma/dma_drv.c` 含 s_dma_raw_cnt，per-file BEHAV 走
  fail-closed defer。**依赖**：无。**与 O 冲突**：无。**许可**：14 文件
  **全部 mit-or-gpl-2.0-only**（codex 第 10 轮 finding 3 订正：原描述
  "1 bsd-3-clause-or-lgpl-2.1-only (pmbus/pmbus_drv.c/.h)" 实为 BC-20
  误植，BC-16 内无 BSD-LGPL/PMBus 子集）。**适配方式**：14 个 BEHAVIORAL 文件
  不作纯重命名结论，按 first_diff 逐文件复核后再决定是否回写 O 命名空间。
  **验证入口**：`run-r16-gate-tests.sh` + 真机 DMA 引擎测试。**工作量**：L3（跨
  BC fail-closed 182 文件部分）。
- **BC-17**（14 defer 文件；BC 默认 defer）：power（fant_devfreq_gov.c/.h/
  fant_gs8601.c/fant_input_event.c/.h/fant_is6608a.c/fant_mp2979a.c/
  fant_mpq8645p.c/fant_virture_chip.c/fant_xdpe12284.c/power.c/power.h/
  power_hw_info.c/.h 中 differs 子集）。**语义**：**已验事实更正**——O 端
  `innopower/inno_input_event.c/h` 已存在 input 子系统（`inno_input_event()`
  回调），F 端 `fant_input_event.c:17-58` 新增 `struct input_handle_private` +
  `fant_input_pm_notifier()` 注册 `register_pm_notifier` +
  PM_HIBERNATION_PREPARE/POST_SUSPEND 等 4 个状态分支 + 蓝牙设备守卫。
  **依赖**：无。**与 O 冲突**：14 文件**全部 unclassified**（codex 第 10 轮
  finding 3 订正：原描述 "13 unclassified" 实为 14，覆盖 power/* 全部 differs
  子集），确权前不可移植。**许可**：unclassified（确权前不可移植，无
  mit-or-gpl 路径）。**适配方式**：若引入 F 版本将与 O 现有 input 子系统
  命名冲突且可能与 R14 suspend 路径冲突。**验证入口**：
  `run-r16-gate-tests.sh` + fantgpu-compat-tests + 真机 s2idle 多轮 +
  input 设备枚举。**工作量**：L3（~6-12 周含 runtime 验证）。
- **BC-18**（8 defer 文件；BC 默认 drop）：vpu（vpu.h/vpu_common.h/vpu_dmabuf.c/
  .h/vpu_drv.c/.h/vpu_for_umd.h/vpu_internal.h 中 differs 子集）。**语义**：F
  厂商 VPU 子系统重命名。**依赖**：无。**与 O 冲突**：无。**许可**：
  mit-or-gpl-2.0-only。**适配方式**：8 个 BEHAVIORAL 文件不作纯重命名结论，
  需逐文件 first_diff、VPU IOCTL 和 UMD ABI 复核后再处置。**验证入口**：
  `run-r16-gate-tests.sh` + 真机 VPU IOCTL。**工作量**：L3（跨 BC fail-closed
  182 文件部分）。
- **BC-19**（2 defer 文件；BC 默认 drop）：smmu（smmu_drv.c/smu_drv.h 中
  differs 子集）。**语义**：F 厂商 SMMU 子系统重命名。**依赖**：无。**与 O
  冲突**：无。**许可**：mit-or-gpl-2.0-only。**适配方式**：2 个 BEHAVIORAL 文件
  不作纯重命名结论，需逐文件 first_diff、SMMU 调用方和 probe 复核后再处置。
  **验证入口**：`run-r16-gate-tests.sh` + 真机 SMMU probe。**工作量**：L3（跨 BC
  fail-closed 182 文件部分）。
- **BC-20**（2 defer 文件；BC 默认 drop）：pmbus（pmbus_drv.c/pmbus_drv.h）。
  **语义**：F 厂商 PMBus 子系统重命名。**依赖**：无。**与 O 冲突**：无。
  **许可**：bsd-3-clause-or-lgpl-2.1-only（履行对应许可文本与 NOTICE 即可）。
  **适配方式**：2 个 BEHAVIORAL 文件不作纯重命名结论；许可已核对不等于语义等价，
  需逐文件 first_diff、PMBus 调用方和总线枚举复核。**验证入口**：
  `run-r16-gate-tests.sh` + 真机 PMBus 总线枚举。**工作量**：L3（跨 BC fail-closed
  182 文件部分）。
- **BC-21**（9 defer 文件；BC 默认 drop）：tools/gpu-info（gpu_info.c/gpu_info.h/
  gpu_info_fantml.c/.h/km.h/gpu_info_y8.c/.h/kgc_gpu_info.c/.h 中 differs 子集）。
  **语义**：F 厂商 GPU info 工具重命名 + 1 新 include；`tools/gpu_info/gpu_info.c`
  含 raw-test，per-file BEHAV 走 fail-closed defer；`kgc_gpu_info.c:23` 新增
  `#include "ftsrv.h"`，O 命名空间下应改 `#include "pvrsrv.h"`。**依赖**：无。
  **与 O 冲突**：含 5 unclassified（`tools/gpu_info/gpu_info_fantml.c/.h/
  fantml_km.h/kgc_gpu_info.c/.h`）。**许可**：mit-or-gpl-2.0-only + 5
  unclassified（确权前不可移植）。**适配方式**：用户态工具可保留 D 命名空间
  使用，per-file BEHAV 走 fail-closed defer。**验证入口**：
  `run-r16-gate-tests.sh` + run-p2-normalize + 真机 gpu_info 工具运行。**工作
  量**：L3（跨 BC fail-closed 182 文件部分）。
- **BC-22a**（2 文件，F-only）：**已验事实更正**——F 端 `fant_stackprotector.c/h`
  无许可证头（仅 `#include` 行），不可推测为 MIT；O 端 Kbuild 使用
  `-fno-stack-protector`，F 端使用 `-fstack-protector-strong`（差异在编译选项
  而非源码）；drop 该 2 文件即保持 O 行为。
- **BC-22b**（1 文件，F-only）：**已验事实更正**——O 端
  `innosrvkm/include/pvr_bridge.h:90` 存在条件 `#include "common_ri_bridge.h"`
  （受 `PVRSRV_ENABLE_GPU_MEMORY_INFO` 控制）；该 header 当前不在 D 树中，但 O
  调用方已就绪。F-only 的 `common_ri_bridge.h` 可作为未来补全入口，但当前
  drop 不引入新授权。

**21 BC / 319 defer 文件收敛原则**（第 11 轮语义证据边界修订）：
1. **BC-level 默认**为该 BC 的整体判定（drop 或 defer）；
2. **per-file fail-closed override** 把 BC 默认 drop 中的 BEHAVIORAL 文件改判为
   defer —— 即"BC drop + per-file BEHAV defer"双层语义；
3. 319 defer 文件 = 116 drop BC 子集中的 BEHAVIORAL 文件（per-file 改判 defer）
   + defer BC 中的全部 differs 子集；
4. 每条 BC 补丁项包含：候选名（簇范围）/语义假设/依赖/与 O 冲突/许可/适配方式/
   验证入口/工作量；语义假设不得替代逐文件裁决；
5. **L0** 只适用于 113 个 `PURE_RENAME` drop + 3 个 F-only drop，共 116 文件；
   混合 BC 的 defer 子集不继承 L0；
6. **L1**（~1 周）：BC-07 的 6 个 BEHAVIORAL/defer 文件（宏/结构差异条件性 port
   + 防御性改动）；
7. **L2**（~2-4 周）：BC-09 的 34、BC-13 的 9、BC-14 的 57 个 defer 文件，共
   100 个（已定位驱动/HAL 差异 + 真机回归）；
8. **L3**（~6-12 周含 runtime 验证）：BC-08/10/17 的 31 个 defer 文件 + 其余
   跨 BC fail-closed 的 182 个 BEHAVIORAL 文件，共 213 个；包含公共 API/PM
   notifier/闭源边界探索、调用方/ABI 复核和 s2idle 多轮验证；
9. **不延期声明**：本节为 P5 路线完整比较的固定产出；执行方按
   `docs/planning/fantgpu-base-update-evaluation.md` P5 路线要求保留 319 文件的
   逐文件 defer 证据和 21 BC 级复核项。若要把文件级语义裁决延期到 P6，须
   dsh/用户在 R16 章程中明确修订 P5 固定产出。

#### per-file 内容审查（codex finding 1/2 返工，2026-09-04 第 7 轮）

按 codex 2026-09-04 P1 finding 1/2："完成 per-file fail-closed 处置并将关键脚本与映射
持久化入 Git / 加入 license 字段；gate 失败须非零退出"。

- **方法（持久化、可重放）**：
  1. `tools/r16-build-bc-map.py`：基于 `tools/p2-normalize-v3.py` 的归一化映射 +
     优先级排序的 BC 分类器（含 BC_08_FILES、BC_09/BC_10/BC_11/BC_12/BC_13/BC_15
     BASENAMES，BC_03/BC_05/BC_07 PREFIXES，BC-22a/BC-22b F-only 专项）→ 输出
     `build/r16-evidence/canon-to-bc.tsv`。
  2. `tools/r16-classify.py`：对每个差异文件做 per-file fnorm 归一化（~30 步
     迭代覆盖 fh2m_fant_/FANT_*/__FANT*_H__/__INNO*_H__/MODULE_AUTHOR/DESC/
     KBUILD_*/桥接 ID/模块名字符串等模式）+ byte compare + 模式分类
     （include guards/copyright/prefix swap/typedef/struct/extern/module info/
     注释/行内调用/bare identifier）→ 输出
     `build/r16-evidence/per-file-classification.tsv` 和
     `build/r16-evidence/per-bc-summary.tsv`，**每条记录含 license_d/license_f
     字段**（来源 `build/r16-license-precheck/{D,F}-license-manifest.tsv`）。
  3. `tools/r16-gate.py`：7 个独立门禁断言，全部使用 `sys.exit(1)` 失败非零
     退出；G0 先校验 432 differs + 3 F-only、raw-row 重复 canonical path 和 manifest
     status 白名单，再执行 G1-G6。
- **per-file fail-closed 处置规则**（不再按 BC 整簇默认）：
  - `cls == 'PURE_RENAME'` → `disposition = drop`
  - `cls == 'BEHAVIORAL'` → `disposition = defer`（**无视 BC 默认**，即使 BC
    默认 drop 也按 defer 处置；codex finding 1 关键修正）
  - `cls == 'F-ONLY'` → `disposition = drop`
  - 其他 → `disposition = defer`（防御性）
- **结果（per-file 门禁 ALL PASS，`python3 tools/r16-gate.py`）**：
  - Gate 0 cardinality = 432 differs + 3 F-only = 435；unknown status = 0；manifest duplicates = 0 ✓
  - Gate 1 assigned = 435 ✓
  - Gate 2 duplicates = 0 ✓（`Counter` 真重复检测，非 `len-set`）
  - Gate 3 BC coverage = 23 ✓
  - Gate 4 per-file coverage = 435 ✓
  - Gate 5 dispositions = {drop: 116, defer: 319}, sum = 435 ✓
  - Gate 6 fail-closed violations = 0 ✓（所有 BEHAVIORAL 已 fail-closed 为 defer）
  - 详情：`build/r16-evidence/{canon-to-bc.tsv,per-file-classification.tsv,
    per-bc-summary.tsv}`（gitignored，由 tracked 脚本+已登记 fixtures 重现）
- **per-BC 处置统计**（依据 `tools/r16-classify.py` 输出 + per-file fail-closed）：

| 处置 | BCs | 文件数 | 说明 |
|------|-----|--------|------|
| drop | 11 | 116 | per-file 审查确认纯重命名（PURE_RENAME）或 F-only。**所有** BEHAVIORAL 已 fail-closed 为 defer，无任何 BC 整簇 drop。 |
| defer | 21 | 319 | fail-closed 处置：含已知行为差异（BC-08/BC-09/BC-10/BC-13/BC-14/BC-17）+ 所有 BC 的 BEHAVIORAL 文件 + 防御性 defer。逐文件 first_diff 样本保留在 `build/r16-evidence/per-file-classification.tsv`。 |

**路径 B 实际成本估算**（per-file fail-closed 修订，2026-09-04 第 7 轮）:
- L0 drop：**116 文件**，零开发成本（仅维护决策记录）。来源：
  `tools/r16-classify.py` 输出 `drop` 处置合计（PURE_RENAME 113 + F-ONLY 3）。
- L1 defer：**6 文件**（BC-07），~1 周（3 宏条件性 port + 防御性改动）。
- L2 defer：**100 文件**（BC-13 9 + BC-09 34 + BC-14 57），~2–4 周
  （hal/驱动的防御性增量 + 真机回归）。
- L3 defer：**213 文件**（BC-08 4 + BC-10 13 + BC-17 14 + 跨 BC 的
  BEHAVIORAL fail-closed 182），~6–12 周（运行时验证、闭源边界、s2idle 多轮
  + 含所有跨 BC 被 fail-closed 的 BEHAVIORAL 文件）。
  - **L3 增量说明**：per-file fail-closed 把原本 BC 整簇 drop 的 BEHAVIORAL
    （如 BC-04 24 / BC-12 6 / BC-21 9 等 BC 默认 drop 中的 BEHAVIORAL）
    全部 fail-closed 入 L3。运行时不可放行时按 BC 整簇 dispose defer。
- L4：无候选（所有行为差异可在 L2/L3 内覆盖）。
- **总计**：435 文件 = 116 drop + 319 defer（6 L1 + 100 L2 + 213 L3），
  累计 ~3-6 月（含真机多轮回归）。

**不可拆分项**：
- 保留 D 5 blob coherent set（`innogpu.o_shipped` 等）+ O wrapper
- 当前 O/R14 已验证稳定（**注意**：P3c stage-000 状态为 `absent + unassessable / runtime-verify`，**尚未执行**；B 的稳定性引用当前 O/R14 证据，非 F 上的 runtime-verify）

**许可成本**：
- selected(F-D) 逐文件审查来源/许可/版权
- **沿用当前许可状态，不新增权利**（载荷不变，授权状态不作扩张；需确认 selected 项许可）
- 工作量取决于 selected 集合大小

**版本策略**（待用户决策）：
- 当前 O 包版本：`4.0.2-i3`，epoch `1788796800`，包名 `innogpu-fh2m-trixie`
- 候选包名：`innogpu-fh2m-trixie`（维持）
- 项目版本序列：下一版本号（如 `4.0.3-i4`，待用户决策）
- 下一审核 epoch：待确定
- dpkg 升级排序：标准升级（同包名，新版本）

**包切换与回退**（中风险）：
- O→O 新版：标准 dpkg 升级
- 全局状态修改：DKMS 重建、initramfs 重建（**注意**：与路径 A 不同，B 不修改 `/usr/bin/Xorg`、`/etc/modules`、ldconfig、modprobe blacklist、用户态载荷；仅重建内核模块和 initramfs）
- **备份需求**：标准系统快照
- **中断恢复**：标准 dpkg 中断恢复
- **回退**：dpkg 降级到旧版本
- **重启后验证**：全栈功能测试（见验证矩阵）

**验证矩阵**（完整验收，同路径 A）：
- 19 项逐项 fixture（P3a/P3b/P3c 全部语义项）
- Debian 6.12 DKMS 构建
- 模块/vermagic/符号验证
- **双 clean-build 可复现**：同输入、同 epoch、两个全新构建根逐字节一致（引用现有发布门禁）
- 包载荷/许可/安装回退验证
- **安装失败立即停止/回退**：dpkg 错误立即停止，触发回退到旧版本，不继续
- 全套现有 CI
- 冷启动、内外屏/热插拔、fbdev/TTY
- **DDX/DRI/GBM/EGL** 全功能验证
- Vulkan/OpenCL/VA-API 全功能
- DMA-BUF/vblank
- s2idle 与 R14 等价 D1-D6
- PVR/内核错误零增长（**R14 具体错误判据**：`3900372`/`PowerLock`/`POWERED_OFF`/新 `PVR_K:(Error)` 均无；PVR 八项计数不增；真实 deep entry/exit；人工画面/键鼠/TTY 判据；区分已归档 pre-existing 日志）
- **目标 PCI ID**：`1ec8:9810`（fh2m GPU，README 和真机证据确认）；验证 F/O 候选均实际绑定该 ID
- selected(F-D) 新差异回归用例（待筛选后确定）

#### 风险对比（基于完整比较 + per-file fail-closed，2026-09-04 第 7 轮）

| 风险类型 | 路径 A | 路径 B |
|---------|--------|--------|
| 技术风险 | **高**（001 构建前提、F 固件兼容性、整栈替换中断恢复、coherent-set 耦合强度 unassessable） | **中**（116 drop 不引入技术风险；L1-L3 319 defer 候选的 BEHAVIORAL 已逐文件 fail-closed，运行时不可放行时按 BC 整簇 dispose defer；BC-08/BC-10 公共 API 变更须先与现有 O pci_drv 调用方协调；BC-17 PM notifier 与 R14 suspend 路径可能冲突） |
| 许可风险 | **高**（需新授权 blob/用户态，且 blob/用户态新授权尚未取得） | **中**（319 defer 实际许可分布：276 mit-or-gpl-2.0-only + 38 unclassified + 2 strictly-confidential + 2 bsd-3-clause-or-lgpl-2.1-only + 1 mit-only；闭源 blob/固件/用户态授权沿用现状，无扩张；F-only 3 文件 drop 后不引入新授权；per-file license 字段已落入 `build/r16-evidence/per-file-classification.tsv`；40 个 unclassified/confidential 需确权前不可移植；3 个 BSD-LGPL/MIT-only 需履行 LICENSE/NOTICE，不属授权缺失） |
| 工程风险 | **高**（全局状态修改多：Xorg/modules/ldconfig/blacklist/用户态/initramfs/DKMS，需备份/回退、命名空间迁移） | **中**（无命名空间迁移；DKMS/initramfs 重建；不修改 Xorg/modules/ldconfig/blacklist；用户态载荷不变；per-file gate 工具入 Git 持久化可重放） |
| 时间风险 | **高**（001 构建前提 + 许可评估 + 全栈验证） | **中**（L0 即时；L1 ~1 周；L2 ~2-4 周；L3 ~6-12 周含 per-file fail-closed 扩列；累计 3-6 月含真机多轮回归） |

#### 路线建议（基于完整比较 + per-file 内容审查）

> **重要修正 (2026-09-04 · codex finding 4)**：之前路径 A 建议基于
> "selected=0 ⇒ 路径 B 实际收益不明确 ⇒ 选 A"的反向推理；但 selected=0 不能
> 反向证明应采用整个 F 栈——理由：
> 1. selected=0 仅说明"B 路线当前无可立即 port 的高价值项"；
> 2. 路径 A 的收益取决于"F 厂商改进对 O 缺失关键功能的覆盖"，与 B 的
>    selected 数无关；
> 3. A 的 4 类风险均为高且 001 构建前提 unassessable，与 B 的 319 defer
>    候选（跨 21 BC）的 L1-L3 工作量不可线性对比。
>
> **因此本节改为基于事实的并列陈述 + 待用户决策**，不再做"建议首选 A"的
> 反向推理。

**事实陈述**（per-file fail-closed 审查 + 路径 B 23 BC labels 完整比较后，2026-09-04 第 7 轮）:
1. **路径 A**：F 厂商整栈替换；工作量为 19 项台账 adapted-port（44 语义项）+
   001 构建前提 + 整栈替换验证；4 类风险均为高；许可成本高且 blob/用户态
   新授权边界尚未取得（与"清晰"无关）；
   **关键收益条件未被当前证据支持**——23 BC labels 中 defer 候选涉及 HPD 守卫、
   shutdown 谓词、PM notifier 等"防御性增量"，未发现"O 缺失的关键功能"
   （per-file PURE 比例仅 116/435 ≈ 27%，其余 319 defer 是尚未完成语义裁决的
   BEHAVIORAL fail-closed 文件）；F 闭源 coherent set 改进（F 的
   `innogpu.o_shipped` 等）的实际收益因 001 构建前提 unassessable 而无法量化。
2. **路径 B**：O 当前基座 + 319 defer 候选（含 L3 增量 182 跨 BC fail-closed
   文件；跨 BC-01..BC-21 共 21 BC）；工作量为 319 defer 候选的运行时验证
   和边界协调（L1-L3，3-6 月）；116 drop 文件零成本；许可风险中
   （319 defer 中 38 unclassified + 2 strictly-confidential = 40 个需确权前不可移植；
   3 个 BSD-LGPL/MIT-only 需履行 LICENSE/NOTICE 义务，不属授权缺失）；无命名空间迁移；F 厂商改进的"防御性增量"因 O 已
   运行稳定（R14 6/6 deep）不构成立即 port 必要性。
3. **per-file 生成/门禁 PASS**：G0-G6 七项门禁全部 ALL PASS（详
   `per-file 内容审查` 小节）；这证明输入组成、映射、许可字段和 fail-closed 处置
   可重放，不等于 319 个 BEHAVIORAL 文件已完成语义裁决。BC-17/BC-22a/BC-22b
   三处 codex 指出事实误判已验确并订正（详 defer 要点 BC-17/-22a/-22b）；关键
   脚本与映射持久化入 `tools/r16-{build-bc-map,classify,gate}.py`，gate 失败非零
   退出。
4. **selected=0 的解读**：当前所有 defer 候选的"功能价值"未被运行时证据
   支持为"必须 port"；这不构成对 F 厂商改进总体的否定，但构成对"B 路线
   立即 port 价值"的否定。F 厂商改进若被认定为"非 cosmetic"，需要 runtime
   证据 + 真机回归数据，当前 P5/P6 阶段无法承担。

**建议**：路径选择**保持用户拍板**（P6 三方定稿阶段）；本节不再做"建议
首选"判断，仅陈述事实和风险边界。

**路径 A 考虑条件**（不变）：
- F 厂商改进项为 O 缺失的关键功能（非 cosmetic/优化）——**完整比较后当前不成立**
- 用户明确要求采用 F 命名空间一致性
- 新授权可获得且成本可接受
- 001 的 16 项 6.12 兼容可完成 adapted-port

**路径 B 保留理由**：
- 当前 O/R14 已验证稳定；B 不引入新风险（116 drop 零成本）
- 319 defer 候选（含跨 BC fail-closed 文件）可作为未来 runtime 验证的
  "候选池"，不是必须立即 port 的清单
- 若未来某 defer 候选经 runtime 验证确有 port 必要性，可增量 port 不影响
  O 整体稳定性
- per-file mapping 持久化入 `tools/` + `build/r16-evidence/`，含 license 字段，
  可由任何复核者执行 `python3 tools/r16-gate.py` 重放核验

**待用户决策项**：
1. 路径 A 候选包名（`fantgpu-fh2m-trixie` 或其他）
2. 路径 A/B 项目版本序列
3. 路径 A/B 下一审核 epoch
4. 是否接受路径 A 的命名空间迁移
5. 是否接受路径 A 的新授权需求
6. 是否接受路径 B 的 selected=0 事实，是否仍选择 B 作为后备

## 下一步

dsh P4 终审已放行（2026-09-04），P0 勘误重开同时裁定。**P5 当前阶段**（A/B 路线表，
初审/终审流程中；P5 尚未放行）。

1. **P5 A/B 路线表** → codex 初审 → dsh 终审（当前阶段）
2. P5 权威文档更新 commit/push（待 dsh 终审后执行）
3. P6 三方定稿 → 用户拍板

## 工具与证据

- **normalization script**：`tools/p2-normalize-v3.py` —— **本轮移入 tracked**（原在 `build/`）；
  输出路径已参数化（`p2-normalize-v3.py [OUTPUT_PATH]`，默认 `build/p2-manifest.tsv`），
  不再写死 `/tmp`；脚本内 `locale.setlocale(LC_ALL, 'C')` 保证确定性排序；已登记 `tools/README.md`。
- **per-file BC mapping 工具集（本轮新增，2026-09-04 第 7 轮）**：
  - `tools/r16-build-bc-map.py`：构建 canonical path → BC 映射，输出
    `build/r16-evidence/canon-to-bc.tsv`。基于 `tools/p2-normalize-v3.py`
    + 优先级排序分类器。
  - `tools/r16-classify.py`：per-file fnorm 归一化 + 字节比较 + 模式分类，
    输出 `build/r16-evidence/per-file-classification.tsv` 和
    `build/r16-evidence/per-bc-summary.tsv`；每行含 license_d/license_f
    字段。
  - `tools/r16-gate.py`：**7 项独立门禁** G0-G6（每项失败 `sys.exit(1)` 非零退出，
    禁止误导性 PASS）：**G0 production cardinality invariant** —— 强制 `differs == 432` 且
    `F-only == 3` 且 `differs+F-only == 435`；拒绝 raw-row duplicate canonical path
    （在 dict 折叠前 `Counter`）；拒绝未知 manifest status（白名单 differs/identical/D-only/
    F-only/F-only-deferred）；**G1** canonical path→BC 映射覆盖（assigned=435，
    差异与未分配立即拒绝）；**G2** BC 唯一性（`Counter` 真重复检测，路径跨 BC 即拒绝）；
    **G3** BC 覆盖（必须含全部 23 个 BC：BC-01..BC-21 + BC-22a + BC-22b）；**G4** per-file 处置
    覆盖（per-file=435）；**G5** disposition/classification/license 白名单 + 总数=435
    （拒绝 UNASSIGNED/MISSING/未知 license/未知 classification/未知 disposition）；
    **G6** fail-closed 纪律（BEHAVIORAL+drop=0，PURE_RENAME+defer=0，
    MISSING+drop=0，MISSING+defer=0）。
    G0 PASS 仅证明输入组成、映射、许可字段和 fail-closed 处置可重放，
    不等于 319 个 BEHAVIORAL 文件已完成语义裁决。
  - 输出：`build/r16-evidence/{canon-to-bc.tsv,per-file-classification.tsv,
    per-bc-summary.tsv}` —— **本机证据，不入库**；由 tracked 脚本 + 已登记
    fixtures 可重放核验。任何复核者执行
    `python3 tools/r16-build-bc-map.py && python3 tools/r16-classify.py &&
    python3 tools/r16-gate.py` 后可对照本节 ALL PASS 声明。
- **manifest**：`build/p2-manifest.tsv` —— 467 行（432 differs + 3 F-only + 31 identical，
  共 466 数据行 + 1 表头），SHA-256 `2eb02ab143e026a89f14c0605f8c5dc449f6657c599645ca59978a854e1058a5`。
  **决定：本机证据，不入库、不在提交范围内。** 依据：`build/` 由 `.gitignore` 的 `/build/`
  规则整目录排除（该目录为可重现的解包/暂存区，由 `binary-manifest.json` + extractor 管理，
  按仓库既有边界从不提交），`git check-ignore -v build/p2-manifest.tsv` 确认命中该规则；
  且该文件可由已入库的脚本逐字节重现，无需入库即可长期核验。任何复核者执行
  `LC_ALL=C python3 tools/p2-normalize-v3.py <任意输出路径>` 后比对上述 SHA-256 即可验证，
  **不必也不应**强制跟踪或改动 `build/` 忽略边界。
- **详细执行记录**：`collab/R16-2026-09-03-基座更新迭代评估/qoder-notes.md`（本机，不入 Git）
- **正式报告**：`collab/R16-2026-09-03-基座更新迭代评估/report.md`（本机，不入 Git）
