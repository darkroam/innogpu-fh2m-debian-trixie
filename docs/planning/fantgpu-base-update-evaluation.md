# fantgpu 基座更新迭代评估（R16）

- **署名**：qoder
- **日期**：2026-09-04
- **状态**：P0 历史放行，现因版权统计勘误重开待 dsh 终审；P1–P3c 原放行结论暂按 P0 勘误
影响分析保留（P1-P3 不依赖版权人计数）；P3c 提交 `f8e9979`。**P4 当前阶段**（整栈可分性
评估，初审/终审流程中；P4 尚未放行）。P5 及其后未授权。

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

## 下一步

dsh P3c 终审已放行（提交 `f8e9979`），并明确「下一步**仅限 P4**；**不得进入 P5**」（`report.md`
节「dsh 终审（P3c · 2026-09-04）：放行，执行 P3c 提交，进入 P4」）。

1. **P4 codex 初审** → dsh 终审（当前阶段）
2. P4 权威文档更新 commit/push（待 dsh 终审后执行）
3. P5 A/B 路线表 —— **须待 P4 终审放行，当前未授权**
4. P6 三方定稿 → 用户拍板

## 工具与证据

- **normalization script**：`tools/p2-normalize-v3.py` —— **本轮移入 tracked**（原在 `build/`）；
  输出路径已参数化（`p2-normalize-v3.py [OUTPUT_PATH]`，默认 `build/p2-manifest.tsv`），
  不再写死 `/tmp`；脚本内 `locale.setlocale(LC_ALL, 'C')` 保证确定性排序；已登记 `tools/README.md`。
- **manifest**：`build/p2-manifest.tsv` —— 467 行（463 对 + 3 F-only），SHA-256
  `2eb02ab143e026a89f14c0605f8c5dc449f6657c599645ca59978a854e1058a5`。
  **决定：本机证据，不入库、不在提交范围内。** 依据：`build/` 由 `.gitignore` 的 `/build/`
  规则整目录排除（该目录为可重现的解包/暂存区，由 `binary-manifest.json` + extractor 管理，
  按仓库既有边界从不提交），`git check-ignore -v build/p2-manifest.tsv` 确认命中该规则；
  且该文件可由已入库的脚本逐字节重现，无需入库即可长期核验。任何复核者执行
  `LC_ALL=C python3 tools/p2-normalize-v3.py <任意输出路径>` 后比对上述 SHA-256 即可验证，
  **不必也不应**强制跟踪或改动 `build/` 忽略边界。
- **详细执行记录**：`collab/R16-2026-09-03-基座更新迭代评估/qoder-notes.md`（本机，不入 Git）
- **正式报告**：`collab/R16-2026-09-03-基座更新迭代评估/report.md`（本机，不入 Git）
