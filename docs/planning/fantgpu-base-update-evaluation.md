# fantgpu 基座更新迭代评估（R16）

- **署名**：qoder
- **日期**：2026-09-04
- **状态**：分两层，不可混同——

**其一 · 检查点结论**：P0、P1、P2、P3a、P3b **均已由 dsh 终审放行**（P3b 见 `report.md`
节「dsh 终审（P3b · 2026-09-04）：放行，仅进入 P3c」；P3a 见「dsh 终审放行」记录）。
P3c 及其后 **未执行、未授权**。

**其二 · 本文档与初始提交的审查状态**：**审查中，尚未放行**。本文档是 2026-09-04
「方案 A」新增的 tracked 资产，其载体与提交本身走独立流转：qoder 产出并提交决定 →
codex 初审（已判**返工**）→ qoder 返工 → 待 codex 复审 → 待 dsh 终审 → 由 dsh 执行
commit/push。在 dsh 终审前不得 commit/push，也不得进入 P3c。

即：下文「已放行」一律指**检查点结论**，不指本文档或本提交已获批准。

## 概述

本文档为 R16 基座更新迭代评估的权威记录，涵盖 19 项台账的 D/O/F 判定矩阵及 P0–P3b 已放行结论。

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

### P3c 台账（6 项，待执行）

| # | Patch | 描述 | 状态 |
|---|-------|------|------|
| 14 | stage-000 | 初始基线 | 待判定 |
| 15 | 003 | 关闭项 | 待判定 |
| 16 | 004 | 关闭项 | 待判定 |
| 17 | 005 | 关闭项 | 待判定 |
| 18 | 008 | 关闭项 | 待判定 |
| 19 | 025-display | 关闭项 | 待判定 |

**P3c 统计**：待执行。

## 汇总

| 检查点 | 语义项总数 | 已覆盖 | 需处理 | 可 drop |
|--------|-----------|--------|--------|---------|
| P3a | 41 | 7 | 34 | 0 |
| P3b | 10 | 0 | 10 | 0 |
| P3c | 待执行 | - | - | - |
| **总计** | **51+** | **7** | **44+** | **0** |

## P0–P3b 已放行结论

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

## 下一步

dsh P3b 终审已放行，并明确「下一步**仅限 P3c**；**不得进入 P4**」（`report.md` 节
「dsh 终审（P3b · 2026-09-04）：放行，仅进入 P3c」）。

1. **本初始提交**：codex 复审 → dsh 终审 → 由 dsh 执行 commit/push（在此之前不提交）
2. **P3c 执行**（stage-000 + 003/004/005/008/025-display 共 6 项销项判定）→ codex 初审 → dsh 终审
3. P4 整栈可分性评估 —— **须待 P3c 终审放行，当前未授权**
4. P5 A/B 路线表
5. P6 三方定稿 → 用户拍板

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
