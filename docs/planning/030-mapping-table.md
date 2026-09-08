# 19 项台账 → Deepin 中性变更记录映射表（O-1 v12 · P5/阶段一/阶段三 三列分离）

> 创建日期：2026-09-05（v5: 2026-09-05 dsh 三阶段返工指令后；v6: 2026-09-05 codex v5 初审 P1 #2 + P2 #6 闭环后；v7: 2026-09-05 codex v6 初审 P1 #1 + P1 #2 + P2 #6 配套修订后；v8: 2026-09-05 codex v7 初审 8 findings 闭环后（F-only 整体后移阶段三 O-4 / O-2 配套修订 / 9 类 symlink 闭合 / tar.zst 可复现规范）；v9: 2026-09-05 codex v8 初审 6 P1 + 2 P2 闭环后配套标签同步（O-1 主体 v8 决策无变化）；v10: 2026-09-05 codex v9 初审 5 P1 + 3 P2 闭环后配套标签同步（O-1 主体 v9 决策无变化）；v11: 2026-09-05 codex v10 初审 4 P1 + 1 P2 闭环后配套标签同步（O-1 主体 v10 决策无变化）；**v12: 2026-09-05 codex v11 初审 2 P1 + 2 P2 闭环后配套标签同步（O-1 主体 v11 决策无变化；per codex v12 P2 #5 要求 v11 残痕统一为 v12，§五 当前状态 v8 → v12，§七 配套记录 v11 → v12）；v13-v24 未修改（codex v12 初审 4 P1 + 1 P2 / codex v13 初审 2 P1 + 2 P2 / codex v14 初审 2 P1 + 2 P2 / codex v15 初审 2 P1 + 1 P2 / codex v16 初审 2 P1 + 1 P2 / codex v17 初审 2 P1 + 2 P2 / codex v18 初审 2 P1 + 1 P2 / codex v19 初审 1 P1 + 1 P2 / codex v20 初审 3 P1 / codex v21 初审 1 P1 / codex v22 初审 1 P1 / codex v23 初审 1 P1 + 1 P2 全部集中在 O-2 snapshot 协议与审计文档，与 O-1 表本体无直接关系）；**2026-09-07 阶段一开工：§三 19 行追溯 BC 反查填实（per-file-classification.tsv 24 canon 命中 + 未匹配参照登记，新增 §三.1/§三.2；版本保持 v12，O-1 全闭合时升版）**）
> 起草：qoder
> 状态：**v12 保持 v11 O-1 主体决策；F-only 3 行保持 excluded-deferred（per codex v7 P1 #1）；v12 仅做配套标签同步（O-1 实质内容未变；codex v9/v10/v11/v12 全部 finding 集中在 O-2 工具 / 框架，与 O-1 表本体无直接关系）**
> 上游：`fantgpu-base-update-evaluation.md` line 349-371（19 Patch 表）+ line 60-69（P3c 子表）+ line 25-50（P3a/P3b 子表）+ line 351-371（语义项列）
> 下游：`docs/planning/030-patch-rederivation-design.md` §四 / 阶段一产物
> **本表所有数值均自 P5 eval ledger 原文反查得到，未估算；TBD 字段均显式标注待哪一步闭合，避免越权。**
> **v6 重大修订**（per codex v5 P1 #2 + P2 #6）：
> 1. **F-only 3 行已删除**（gpu/fant_stackprotector.{c,h}、srvkm/include/common_ri_bridge.h）—— 它们不能由 D + 000-029 重放产生，违反阶段一禁止读取 F0 的边界；保留为**阶段三输入占位**（详 §四 单独段）；
> 2. **P5 原始判定 / 阶段一 Deepin 处置 / 阶段三 F0 决策 三列分离** —— P5 的 `adapted-port / retain / runtime-verify` 与阶段一的 `applied / duplicate / no-op / pure-rename / invalid` 不再混用同一列；
> 3. **19 行 TBD 显式标注 reverse-lookup 路径** —— 每个 TBD 字段给出明确反查来源（per-file-classification.tsv / BC 矩阵 / O-2 输出等），不得"待裁决"模糊处理。

## 一、19 项台账来源（P5 原文不可变 · 三列分离）

来源：`fantgpu-base-update-evaluation.md` line 349-371 补丁清单（P5 终审定稿）。

**v6 三列分离（per codex P2 #6）**：

- **P5 原始判定**列 = P5 line 351-371 `Patch 级处置` 原文（**仅供追溯，不作为阶段一决策依据**）；
- **阶段一 终判**列 = 阶段一 O-1 闭合后的 Deepin 处置（`applied` / `duplicate` /
  `no-op` / `pure-rename` / `invalid`），**不包含**任何 F0 决策字段；
- **阶段三 F0 决策**列 = 阶段三 O-3 + O-4 闭合后才填入（`absorb` / `adapt` /
  `rewrite` / `base-retain` / `no-030`），**当前全部 TBD**。

| # | Patch | P5 原始判定（P5 line 351-371） | 阶段一 终判（Deepin 处置） | 阶段三 F0 决策 |
|---|-------|--------------------------------|---------------------------|----------------|
| 1 | 001 | adapted-port 16 项（构建前提） | TBD（待阶段一 O-1 闭合 + O-2 证据） | TBD（阶段三 O-4 + O-3 启动后） |
| 2 | 002 | adapted-port 2 项（F 完全缺失） | TBD | TBD |
| 3 | 006 | 整体 adapted-port + reconcile（F 已覆盖 3 sem-eq + 1 partial） | TBD | TBD |
| 4 | 007 | adapted-port 1 项（F 完全缺失） | TBD | TBD |
| 5 | 009 | adapted-port 4 项（F 完全缺失） | TBD | TBD |
| 6 | 023 | adapted-port 5 项（运行时性能影响需实测） | TBD（运行时部分按框架 §七.1 走 UNVERIFIED） | TBD |
| 7 | 025-dma | adapted-port 2 项（DMA resv usage R/W） | TBD | TBD |
| 8 | 026-vblank | adapted-port 1 项（inactive CRTC guard） | TBD | TBD |
| 9 | 027 | adapted-port 3 项 + reconcile 1 项（prime_import 类型混淆） | TBD | TBD |
| 10 | 024 | adapted-port 1 项（DVFS 协调前提） | TBD | TBD |
| 11 | 026-lifecycle | adapted-port 3 项（DVFS suspend/resume） | TBD | TBD |
| 12 | 028 | adapted-port 4 项（F 基础设施存在但未连接） | TBD | TBD |
| 13 | 029 | adapted-port 2 项（F 隐式排除，缺显式逻辑） | TBD | TBD |
| 14 | stage-000 | runtime-verify（初始基线验证，**尚未执行**） | TBD（运行时部分按框架 §七.1 走 UNVERIFIED） | TBD |
| 15 | 003 | retain（关闭项，P5 line 124：所检查局部语义与 O 一致） | TBD（候选 `retain` / `no-op` 之一；**不预设**） | TBD |
| 16 | 004 | retain（关闭项） | TBD（同上） | TBD |
| 17 | 005 | retain（关闭项） | TBD（同上） | TBD |
| 18 | 008 | retain（关闭项） | TBD（同上） | TBD |
| 19 | 025-display | runtime-verify（关闭项，根因假设未证实/证伪） | TBD（运行时部分按框架 §七.1 走 UNVERIFIED） | TBD |

**台账合计 = 19**（P5 不可变）。

**F-only 3 文件已从本表删除**（per codex P1 #2）：详 §四 单独段。

**P5 原始判定分布（仅供追溯）**：

- adapted-port：13 patch（#1-13，含 #3 006 整体 adapted-port + reconcile，
  #9 027 adapted-port + reconcile）；
- retain：4 patch（#15-18: 003/004/005/008）；
- runtime-verify：2 patch（#14 stage-000、#19 025-display）。

**P5 语义裁决记录合计 = 59**（来自 P5 line 78 校验 7+6+2+44=59 ✓，
来自 18+2+4+1+4+5+2+1+4+1+3+4+2+1+2+2+1+1+1 = 59，详 P5 line 78）。

## 二、Deepin 中性变更记录字段模板（每条必填）

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| 编号 | D-NNN | 三位数（`D001` 起递增）；与 P5 Patch 编号无强制对应 |
| 来源台账项 | ref | 19 项台账中的项号（`001` / `002` / ... / `025-display`） |
| 来源 P5 语义裁决记录 | ref | 59 语义裁决记录中的项号（来自 P5 line 351-371 `语义项` 列） |
| 来源 patch | ref | 000-029 patch 系列中的具体 patch 编号 + SHA-256 |
| 目标路径 | path | D_stage 源树中的具体路径（**仅 D 端**，不涉及 F0） |
| 语义（修改做什么、为什么、改动范围） | text | 修改内容描述 + 原因 + 影响范围 |
| 依赖（前序 patch / 反向 patch / 共享结构 / 共享回调） | text | 与其他 patch 的依赖关系 |
| 顺序（应用顺序约束） | text | 必须在哪些 patch 之前 / 之后应用 |
| 许可证证据（来源 SPDX + 上游 notice） | text | `tools/audit-licenses.py` 输出 + 上游 notice 引用 |
| 静态验证入口 | enum+ref | 见框架 §七.1（仅阶段二 fixture 子集） |
| 运行时验证入口 | enum+ref | 见框架 §七.1（仅阶段二 fixture 子集） |
| 处置 | enum | **`applied`** / **`duplicate`** / **`no-op`** / **`pure-rename`** / **`invalid`**（**不包含**任何 F0 决策字段） |
| 状态 | enum | `候选` → `已闭合` → `可重放` |
| 双向追溯校验 | text | 该变更记录与 D → D_stage 源树之间是否完成反向追溯（依赖 O-2） |
| **F0 比较字段** | — | **禁止填写**（推迟到阶段三） |
| **030-NNN 字段** | — | **禁止填写**（推迟到阶段三） |

## 三、19 项台账 → Deepin 中性变更记录映射骨架（v6 三列分离 + reverse-lookup 路径）

**字段约定（v6 重定义；per codex P2 #6）**：

- **P5 原始判定**列：从 P5 line 351-371 `Patch 级处置` 反查得到（**仅追溯**，
  不作为阶段一决策依据）；
- **阶段一 终判**列：阶段一 O-1 闭合后的 Deepin 处置（`applied` / `duplicate` /
  `no-op` / `pure-rename` / `invalid`），**不包含**任何 F0 决策字段；
- **阶段三 F0 决策**列：阶段三 O-3 + O-4 闭合后才填入（`absorb` / `adapt` /
  `rewrite` / `base-retain` / `no-030`），**当前全部 TBD**；
- **追溯 BC**：`fantgpu-base-update-evaluation.md` line 470-492 的 23 BC 标签
  （仅用于追溯 D 端文件归属，**不涉及 F0**）；
- **追溯 per-file 证据**：P5 BC 矩阵的 per-file 列表 +
  `tools/r16-classify.py` 输出 8 字段；
- **双向追溯校验**（v6 强化）：标记该 Patch 与 D_stage 实际文件之间是否完成
  反向追溯，依赖 O-2 闭合；
- **reverse-lookup 路径**（v6 新增）：每个 TBD 字段必须显式给出反查来源
  （`per-file-classification.tsv` 行号 / 23 BC 矩阵 / O-2 输出路径 /
  D_stage 物化根位置），**禁止**"待裁决"模糊处理。

### 19 Patch → Deepin 中性变更记录映射表

| # | Patch | 语义裁决记录数 | P5 原始判定 | 阶段一 终判 + reverse-lookup 路径 | 阶段三 F0 决策 | 双向追溯校验 | 状态 |
|---|-------|---------------|-------------|--------------------------------|----------------|-------------|------|
| 1 | 001 | 18（P5 line 353） | adapted-port 16 项（构建前提） | TBD（待 O-2 证据终判）· 追溯 BC 已反查（2026-09-07 非估算）：BC-09×7 + BC-01 + BC-13 + BC-15 + BC-18 + BC-19 + BC-20（13 文件命中 + 10 未匹配参照，详 §三.1/§三.2）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合（O-2 待启动） | 候选 |
| 2 | 002 | 2（P5 line 354） | adapted-port 2 项（F 完全缺失） | TBD · 追溯 BC 已反查：BC-09×2（srvkm/dpu_connector.c、srvkm/dpu_dp.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 3 | 006 | 4（P5 line 355，含 3 sem-eq + 1 partial + reconcile） | 整体 adapted-port + reconcile | TBD · 追溯 BC 已反查：BC-09（srvkm/dpu_connector.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 4 | 007 | 1（P5 line 356） | adapted-port 1 项 | TBD · 追溯 BC 已反查：BC-09（srvkm/dpu_drm_fb.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 5 | 009 | 4（P5 line 357） | adapted-port 4 项 | TBD · 追溯 BC 已反查：BC-09（srvkm/dpu_connector.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 6 | 023 | 5（P5 line 358，运行时性能影响） | adapted-port 5 项 | TBD · 追溯 BC 已反查：BC-01（srvkm/include/dpu_drm_gem.h）+ BC-09（srvkm/dpu_drm_gem.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |
| 7 | 025-dma | 2（P5 line 359，DMA resv usage R/W） | adapted-port 2 项 | TBD · 追溯 BC 已反查：BC-01（srvkm/include/dpu_compatibility.h）+ BC-09（srvkm/dpu_drm_gem.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 8 | 026-vblank | 1（P5 line 360，inactive CRTC guard） | adapted-port 1 项 | TBD · 追溯 BC 已反查：BC-09（srvkm/pdp0_crtc.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 9 | 027 | 4（P5 line 361，prime_import 类型混淆 + reconcile） | adapted-port 3 项 + reconcile | TBD · 追溯 BC 已反查：BC-09（srvkm/dpu_drm_gem.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 10 | 024 | 1（P5 line 362，DVFS 协调前提） | adapted-port 1 项 | TBD · 追溯 BC 已反查：BC-10（srvkm/pvr_dvfs_device.c → TSV 无 inno 行，参照 srvkm/ft_dvfs_device.c = BC-10 defer/BEHAVIORAL，详 §三.2）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 11 | 026-lifecycle | 3（P5 line 363，DVFS suspend/resume） | adapted-port 3 项 | TBD · 追溯 BC 已反查：BC-10（srvkm/pvr_drm.c → 参照 srvkm/ft_drm.c = BC-10 defer/BEHAVIORAL，详 §三.2）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |
| 12 | 028 | 4（P5 line 364，原子计数协调） | adapted-port 4 项 | TBD · 追溯 BC 已反查：BC-13（gpu/hal.h）+ BC-15（gpu/gpu_pci_drv.c）+ BC-10 参照（srvkm/pvr_drm.c → ft_drm.c，详 §三.2）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 13 | 029 | 2（P5 line 365，DDCCI 显式逻辑） | adapted-port 2 项 | TBD · 追溯 BC 已反查：BC-09×2（srvkm/dpu_dp_debugfs.c、srvkm/dpu_panel_backlight.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3 | TBD | 未闭合 | 候选 |
| 14 | stage-000 | 1（P5 line 366，初始基线，**尚未执行**） | runtime-verify | TBD · 追溯 BC：跨 BC 范围（无 patch 文件，基线验证项，不映射单文件；详 §三.1）· O-2 differs 行反查待批次3；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |
| 15 | 003 | 2（P5 line 367，P3c 子表 + line 124） | retain（关闭项） | TBD · 追溯 BC 已反查：BC-09×2（srvkm/dpu_connector.c、srvkm/dpu_panel_backlight.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3（候选 `retain` / `no-op` 之一；**不预设**） | TBD | 未闭合 | 候选 |
| 16 | 004 | 2（P5 line 368，P3c 子表 + line 124） | retain（关闭项） | TBD · 追溯 BC 已反查：BC-09×2（srvkm/dpu_panel_pwr.c、srvkm/dpu_panel_backlight.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3（004 为 P3a 编号） | TBD | 未闭合 | 候选 |
| 17 | 005 | 1（P5 line 369，P3c 子表 + line 124） | retain（关闭项） | TBD · 追溯 BC 已反查：BC-09（srvkm/dpu_panel_backlight.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3（005 为 P3a 编号） | TBD | 未闭合 | 候选 |
| 18 | 008 | 1（P5 line 370，P3c 子表 + line 124） | retain（关闭项） | TBD · 追溯 BC 已反查：BC-09（srvkm/gpu_drm.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3（008 为 P3a 编号） | TBD | 未闭合 | 候选 |
| 19 | 025-display | 1（P5 line 371，根因假设未证实/证伪） | runtime-verify | TBD · 追溯 BC 已反查：BC-09（srvkm/dpu_drm_pm.c）· 来源 patch + SHA + 顺序见 §三.3 · O-2 differs 行反查待批次3；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |

### 三.1 追溯 BC 反查明细（2026-09-07 填实，非估算）

反查来源：`build/r16-evidence/per-file-classification.tsv`（435 行，
`tools/r16-classify.py` 输出，`tools/r16-gate.py` 验证；canon_path 为
inno_ 前缀剥离后的规范路径）。全部命中行 disposition = `defer`、
classification = `BEHAVIORAL`（无 drop/PURE_RENAME 命中）。

| 台账项 | D 端目标文件 → canon 路径 | BC |
|---|---|---|
| 001 | innodpu_dp.c→srvkm/dpu_dp.c、innodpu_drm_modeset.c→srvkm/dpu_drm_modeset.c、innodpu_hdmi.c→srvkm/dpu_hdmi.c、innodpu_panel_backlight.c→srvkm/dpu_panel_backlight.c、innodpu_vga.c→srvkm/dpu_vga.c、innogpu_drm.c→srvkm/gpu_drm.c、gen_g3_ne_hdmi.c→srvkm/gen_g3_ne_hdmi.c（7 文件） | BC-09 |
| 001 | include/rogue_trace_events.h→srvkm/include/rogue_trace_events.h | BC-01 |
| 001 | hal_power.c→gpu/hal_power.c | BC-13 |
| 001 | innogpu_pci_drv.c→gpu/gpu_pci_drv.c | BC-15 |
| 001 | innopmbus_drv.c→pmbus/pmbus_drv.c | BC-20 |
| 001 | innosmmu_drv.c→smmu/smmu_drv.c | BC-19 |
| 001 | innovpu_drv.c→vpu/vpu_drv.c | BC-18 |
| 002 | innodpu_connector.c→srvkm/dpu_connector.c、innodpu_dp.c→srvkm/dpu_dp.c | BC-09 ×2 |
| 003（关闭项） | innodpu_connector.c、innodpu_panel_backlight.c（同上二 canon） | BC-09 ×2 |
| 004（关闭项） | innodpu_panel_pwr.c→srvkm/dpu_panel_pwr.c、innodpu_panel_backlight.c | BC-09 ×2 |
| 005（关闭项） | innodpu_panel_backlight.c | BC-09 |
| 006 | innodpu_connector.c→srvkm/dpu_connector.c | BC-09 |
| 007 | innodpu_drm_fb.c→srvkm/dpu_drm_fb.c | BC-09 |
| 008（关闭项） | innogpu_drm.c→srvkm/gpu_drm.c | BC-09 |
| 009 | innodpu_connector.c | BC-09 |
| 023 | include/innodpu_drm_gem.h→srvkm/include/dpu_drm_gem.h | BC-01 |
| 023 | innodpu_drm_gem.c→srvkm/dpu_drm_gem.c | BC-09 |
| 025-dma | include/innodpu_compatibility.h→srvkm/include/dpu_compatibility.h | BC-01 |
| 025-dma | innodpu_drm_gem.c→srvkm/dpu_drm_gem.c | BC-09 |
| 026-vblank | pdp0_crtc.c→srvkm/pdp0_crtc.c | BC-09 |
| 027 | innodpu_drm_gem.c→srvkm/dpu_drm_gem.c | BC-09 |
| 024 | pvr_dvfs_device.c（TSV 无 inno 行）→ 参照 srvkm/ft_dvfs_device.c | BC-10（参照） |
| 025-display | innodpu_drm_pm.c→srvkm/dpu_drm_pm.c | BC-09 |
| 026-lifecycle | pvr_drm.c（TSV 无 inno 行）→ 参照 srvkm/ft_drm.c | BC-10（参照） |
| 028 | hal.h→gpu/hal.h | BC-13 |
| 028 | innogpu_pci_drv.c→gpu/gpu_pci_drv.c | BC-15 |
| 028 | pvr_drm.c → 参照 srvkm/ft_drm.c | BC-10（参照） |
| 029 | innodpu_dp_debugfs.c→srvkm/dpu_dp_debugfs.c、innodpu_panel_backlight.c | BC-09 ×2 |
| stage-000 | 无 patch 文件（初始基线验证） | 跨 BC 范围（不映射单文件） |

### 三.2 未匹配清单与参照行（2026-09-07 登记）

TSV 对 `pvr_*` 与部分 `inno_*.c` 采用 F 侧命名行；以下 D 端文件无直接
TSV 行，按"同结构 F 侧行"登记为追溯参照（**仅追溯 D 端归属，不读取
F0 内容、不预判阶段三决策**）：

| D 端文件 | 所属台账 | TSV 参照行 | BC |
|---|---|---|---|
| srvkm/pvr_drm.c | 001 / 026-lifecycle / 028 | srvkm/ft_drm.c | BC-10 |
| srvkm/pvr_dvfs_device.c | 024 | srvkm/ft_dvfs_device.c | BC-10 |
| srvkm/include/pvr_fence_trace.h | 001 | srvkm/include/ft_fence_trace.h | BC-03 |
| gpu/inno_drm.c、inno_mm.c、inno_pci.c、inno_task.c、inno_uuid.c（001） | 001 | gpu/fant_drm.c、fant_mm.c、fant_pci.c、fant_task.c、fant_uuid.c | BC-14 ×5 |
| innopower/inno_devfreq_gov.c | 001 | power/fant_devfreq_gov.c | BC-17 |
| Kbuild（001 的 -Wno-error 豁免） | 001 | 无 TSV 行（归 build-metadata，非源码文件） | — |
| gpu/compat_kernel6.h（001 新增文件） | 001 | 无 TSV 行（D 端新增，F 侧无对应） | — |

### 三.3 来源 patch SHA-256 与顺序（2026-09-07 填实；SHA 待 sha256sum 实测复核）

SHA-256 来源：`docs/planning/patch-provenance.md` 记录值（9 项启用 +
4 项关闭）+ 工具 SHA（patch-000）。5 项 suspend patch 的 SHA 待
sha256sum 实测（批次1b，需 Bash）；9 项记录值同样待实测复核后再认定
为"已填实"。顺序约束来源：patch-provenance 启用集合（patched-27 开关
集合）+ docs/patches/024-suspend-resume.md 构建接线记录。

| 台账项 | 来源 patch（文件） | SHA-256（记录值/待实测） | 顺序约束 |
|---|---|---|---|
| 1 001 | `patches/001-kernel-6.12-compat.patch` | be5c8ae9e08f5a2979e939bd18d4f9cc35593c5333fe80b1fb7748ffdcf71ab5（待复核） | **最先应用**（构建前提；Kbuild -Wno-error 豁免归 build-metadata） |
| 2 002 | `patches/002-dp-fbdev-fallback-mode.patch` | 1a12de65f201839232a99f542707f43240b175bbe80029926b4ed3ab180f7329（待复核） | patched-27 启用集；001 之后 |
| 3 006 | `patches/006-local-connector-acpi-map.patch` | 63a6569ccb13adfadc7d717cf0b1bc6a338ff7f0231429e6b9d266b0a07340b5（待复核） | patched-27 启用集 |
| 4 007 | `patches/007-fbdev-io-mmap.patch` | 1adb7a3744abb936d41e4c6e54f490f4535b3a2e344d0538aa9368a824337733（待复核） | patched-27 启用集 |
| 5 009 | `patches/009-local-internal-edp-connector.patch` | e6b955fd3cbde69f211098108c2770fdce8d8eb052096ee20d03b9522e5bb26c（待复核） | patched-27 启用集（**builder 序：009 先于 007**，per scripts/build-deepin-coherent.sh） |
| 6 023 | `patches/023-invisible-read-no-writeback.patch` | ea35a852d3b0d2818cd1abfbf20c888eacaccdc87371fd9a20f9520d5ee01f63（待复核） | patched-27 启用集 |
| 7 025-dma | `patches/025-dma-resv-usage-rw.patch` | 05de1bdd503d3a83d82b7e0de44d74535c39d02ae153be4649c90e0c1ae0a027（待复核） | patched-27 启用集 |
| 8 026-vblank | `patches/026-inactive-crtc-vblank-guard.patch` | 864bc3d651250ed9b701d376f6408fe7f506b2fea5fe7129007034e95a80216b（待复核） | patched-27 启用集 |
| 9 027 | `patches/027-foreign-dmabuf-lifecycle.patch` | ab2d1b418fa315fc0528c65e425ad37e16ecdde9ae81066869a487a5c6ce7ca5（待复核） | patched-27 启用集 |
| 10 024 | `patches/024-suspend-resume.patch` | TBD（sha256sum 待 Bash） | suspend 组：builder 序在 **023 后、025 前**（build-deepin-coherent.sh），先于 026-lifecycle / 028 / 029（防御性快速路径） |
| 11 026-lifecycle | `patches/026-suspend-resume-dvfs-lifecycle.patch` | TBD | suspend 组：024 之后 |
| 12 028 | `patches/028-suspend-resume-hal-temp-monitor-delay.patch` | TBD | suspend 组：024 之后 |
| 13 029 | `patches/029-suspend-resume-ddcci-panel.patch` | TBD | suspend 组：4.0.2-i3 源树配方 026-lifecycle → 028 → 029（per scripts/build-innogpu-driver.sh，DDCCI 退场验证前置） |
| 14 stage-000 | `tools/patch-gpupll-object.py`（binary-transform，无 .patch 文件）+ Deepin 基线导入 | e5f9ee94f55aed2507359d0708f2d88b5b3251582380f5792cab54892a35274a（工具 SHA） | 基线最先；对象单点字节变换在构建期执行 |
| 15 003 | `patches/003-panel-backlight-fallback.patch` | 8cd6b492b01e2c42c3eb6dfa8d7042bc2e5f57654159dc579a11a66d6a2c6f7b（待复核） | **关闭项**（patched-27 集合外，不应用） |
| 16 004 | `patches/004-panel-platform-fallback.patch` | 330c3a06998400bb4235385ccaff24a27dc319df508f613eeaf7a80b42814513（待复核） | 关闭项 |
| 17 005 | `patches/005-backlight-initial-enable.patch` | 9fee230ceb3347b05c107bdb4454fd8d19f3ee32d19f194c7f492526d6deec15（待复核） | 关闭项 |
| 18 008 | `patches/008-pvr-init-diagnostic.patch` | 4cfd545afcfbac337e8a018cfaea4f079d28d0332be6cddfea167cb84bfb6394（待复核） | 关闭项 |
| 19 025-display | `patches/025-suspend-resume-display.patch` | TBD | 4.0.1-i4 曾启用；4.0.2-i3 **关闭**（runtime-verify，根因假设未证实/证伪） |

### 三.4 Deepin 中性变更记录展开（D-NNN 骨架 · 2026-09-07）

按 §二 字段模板展开：每个实际生效修改（启用 patch）= 1 条记录；关闭项
与 runtime-verify 项同样登记（处置待 O-2 证据终判，**不预设**）。字段
填写状态：语义/目标路径/顺序 = 已自 patch 文档反查填实；来源 P5 语义
裁决记录 = 引用 P5 line（追溯用，**不引入 F0 决策**）；**许可证证据
（D 端）** = 全部 patch 目标文件 per-file-classification.tsv license_d
= `mit-or-gpl-2.0-only`（§三.1 命中行反查；正式证据待
`tools/audit-licenses.py` 运行时输出回填，未匹配参照行见 §三.2）；
验证入口 = 框架 §七.1 阶段二 fixture 子集（阶段二启动时填实）；
处置 / 状态 / 双向追溯校验 = 待批次3/4。

| D-NNN | 来源台账项 | 来源 P5 语义裁决记录 | 来源 patch | 目标路径（D_stage 端，简） | 语义（简） | 顺序 | 处置 | 状态 |
|---|---|---|---|---|---|---|---|---|
| D001 | stage-000 | P5 line 366 | 基线导入 + `tools/patch-gpupll-object.py`（000） | 整树基线 + innogpu/innogpu.o_shipped 单点字节 | Deepin 202504 基线 + GPU PLL 首字节跳过（构建期确定性变换） | 最先 | TBD（待 O-2） | 候选 |
| D002 | 001 | P5 line 353（18 项） | `patches/001-kernel-6.12-compat.patch` | Kbuild + innogpu/*（6）+ innopmbus/innopower/innosmmu/innovpu + innosrvkm 11 文件 | Kernel 6.12 兼容（构建前提；Kbuild -Wno-error 归 build-metadata） | 最先 | TBD | 候选 |
| D003 | 002 | P5 line 354（2 项） | `patches/002-dp-fbdev-fallback-mode.patch` | innosrvkm/innodpu_connector.c、innodpu_dp.c | DP fbdev 回退模式 | 001 后 | TBD | 候选 |
| D004 | 006 | P5 line 355（4 项） | `patches/006-local-connector-acpi-map.patch` | innosrvkm/innodpu_connector.c | 本地 connector ACPI 映射（device-profile 边界） | builder 序 006 | TBD | 候选 |
| D005 | 007 | P5 line 356（1 项） | `patches/007-fbdev-io-mmap.patch` | innosrvkm/innodpu_drm_fb.c | fbdev io mmap | builder 序 009 之后 | TBD | 候选 |
| D006 | 009 | P5 line 357（4 项） | `patches/009-local-internal-edp-connector.patch` | innosrvkm/innodpu_connector.c | 本地内接 eDP connector（device-profile 边界） | builder 序 007 之前 | TBD | 候选 |
| D007 | 023 | P5 line 358（5 项） | `patches/023-invisible-read-no-writeback.patch` | innosrvkm/innodpu_drm_gem.c + include/innodpu_drm_gem.h | invisible GEM 读不写回 | builder 序 023 | TBD | 候选 |
| D008 | 025-dma | P5 line 359（2 项） | `patches/025-dma-resv-usage-rw.patch` | innosrvkm/innodpu_drm_gem.c + include/innodpu_compatibility.h | dma_resv usage R/W 语义 | builder 序 024 之后 | TBD | 候选 |
| D009 | 026-vblank | P5 line 360（1 项） | `patches/026-inactive-crtc-vblank-guard.patch` | innosrvkm/pdp0_crtc.c | inactive CRTC vblank 守卫 | builder 序 026 | TBD | 候选 |
| D010 | 027 | P5 line 361（4 项） | `patches/027-foreign-dmabuf-lifecycle.patch` | innosrvkm/innodpu_drm_gem.c | foreign dmabuf 生命周期（prime_import 类型混淆修复） | builder 序 027 | TBD | 候选 |
| D011 | 024 | P5 line 362（1 项） | `patches/024-suspend-resume.patch` | innosrvkm/pvr_dvfs_device.c | deep S3 唤醒 devfreq 电源状态门禁（防御性快速路径） | builder 序 023 后、025 前 | TBD | 候选 |
| D012 | 026-lifecycle | P5 line 363（3 项） | `patches/026-suspend-resume-dvfs-lifecycle.patch` | innosrvkm/pvr_drm.c | DVFS suspend/resume 生命周期同步 | i3 组第一 | TBD | 候选 |
| D013 | 028 | P5 line 364（4 项） | `patches/028-suspend-resume-hal-temp-monitor-delay.patch` | gpu/hal.h + gpu/innogpu_pci_drv.c + pvr_drm.c | HAL 温度 work 门禁（原子计数协调） | i3 组第二 | TBD | 候选 |
| D014 | 029 | P5 line 365（2 项） | `patches/029-suspend-resume-ddcci-panel.patch` | innosrvkm/innodpu_dp_debugfs.c + innodpu_panel_backlight.c | DDCCI panel 显式逻辑 | i3 组第三 | TBD | 候选 |
| D015 | 003 | P5 line 367（2 项） | `patches/003-panel-backlight-fallback.patch` | innodpu_connector.c、innodpu_panel_backlight.c | 关闭项（patched-27 集合外） | 不应用 | TBD（候选 retain/no-op；**不预设**） | 候选 |
| D016 | 004 | P5 line 368（2 项） | `patches/004-panel-platform-fallback.patch` | innodpu_panel_pwr.c、innodpu_panel_backlight.c | 关闭项 | 不应用 | TBD（同上） | 候选 |
| D017 | 005 | P5 line 369（1 项） | `patches/005-backlight-initial-enable.patch` | innodpu_panel_backlight.c | 关闭项 | 不应用 | TBD（同上） | 候选 |
| D018 | 008 | P5 line 370（1 项） | `patches/008-pvr-init-diagnostic.patch` | innogpu_drm.c | 关闭项 | 不应用 | TBD（同上） | 候选 |
| D019 | 025-display | P5 line 371（1 项） | `patches/025-suspend-resume-display.patch` | innosrvkm/innodpu_drm_pm.c | 关闭项（i4-only；根因假设未证实/证伪） | i3 不应用 | TBD（runtime-verify 口径） | 候选 |

**验证入口（阶段二 fixture 子集，per 框架 §7.1；阶段二启动时填实执行结果）**：

- D001 → 双 clean-build 字节一致 + 包载荷/许可 + `tests/runtime/run-capability-baseline.sh`；
- D002 → 编译通过 + 模块 vermagic/符号（`tests/unit/run-r16-build-bc-map-tests.sh` 编译套件）；
- D003 / D004 / D006 → `tools/probe-drm-topology.c`（DRM device open + 内接 eDP/ACPI 映射观测）；
- D005 → DRM open + fbdev mmap（fbdev mmap 集成维持 UNVERIFIED，per §7.2 BC-09 口径）；
- D007 → `tools/probe-pdp-invisible-read.c`（READ 不写回断言）；
- D008 / D010 → `tools/run-dmabuf-regression-test.sh`（self-import 子集；foreign/cross-device PRIME 维持 UNVERIFIED）；
- D009 → `tools/probe-drm-vblank.c`（inactive CRTC 守卫，预期快速 EINVAL）；
- D011 / D012 / D013 / D014 → `tests/unit/run-suspend-resume-tests.sh` + `tools/probe-suspend-resume-state.sh`（hal suspend/resume、rail gating 真机 UNVERIFIED）；
- D015-D019（关闭项）→ 阶段一无 fixture（阶段三 P3c 裁决口径：retain / runtime-verify）。

### 字段差异（v6 vs v5）

| 字段 | v5 阶段一 | v6 阶段一 |
| --- | --- | --- |
| **P5 原始判定**列 | 与阶段一处置混用一列 | **独立列**（仅追溯，不作为阶段一决策依据） |
| **阶段一 终判**列 | 单列 | **独立列**（Deepin 处置：applied / duplicate / no-op / pure-rename / invalid） |
| **阶段三 F0 决策**列 | 未列出 | **新增独立列**（TBD；推迟到阶段三 O-3 + O-4 闭合） |
| F-only 3 行（v5 表） | 标 `applied`（v5 §三 末段 + §四 6） | **删除**（v6 §三 不再列；F-only 移到 §四 阶段三输入占位） |
| **reverse-lookup 路径**（v6 新增） | TBD（无路径） | **每行显式标注**（per-file-classification.tsv / BC 矩阵 / O-2 输出路径） |
| 候选 030-NNN ID | 估算（adapted-port / retain / F-only 行） | **删除**（推迟到阶段三） |
| 类目（absorb / adapt / rewrite / base-retain / no-030） | 预填 retain / F-only；TBD adapted-port | **删除**（推迟到阶段三） |
| F0 文件 SHA-256 | 含字段（O-4 锁定后填入） | **删除**（推迟到阶段三） |
| 来源 patch（000-029 patch 编号 + SHA-256） | 未列出 | **新增**（Deepin 血统 patch 唯一标识） |
| 顺序（应用顺序约束） | 未列出 | **新增**（中性变更记录的可重放性所需） |

### F-only 3 文件已从阶段一映射删除（per codex P1 #2）

详 §四 单独段（阶段三输入占位）。**v5 §三 末段的"F-only per-file 证据
applied" 处置已删除** —— F-only 文件不能由 D + 000-029 重放产生，违反
阶段一禁止读取 F0 的边界。

## 四、阶段三输入占位：F-only 3 文件（per codex P1 #2）

> **v5 错误已纠正**：v5 §三 末段将 F-only 3 文件标 `applied`、§四 第 6 项
> 要求"3 个 F-only per-file 证据行按 BC-22a / BC-22b 归属预填 `applied`
> 处置" —— **这些全部违反阶段一禁止读取 F0 的硬边界**，已被 v6 删除。

**F-only 定义（不可在阶段一应用）**：P5 per-file 证据中分类为 `F-only` 的
文件 = D 中不存在而 F0 中存在的文件（P5 line 491-492 + `tools/r16-classify.py`
输出）。F-only 不能由 D + 000-029 重放产生，**必须**作为阶段三输入待裁决
记录，**不进入**阶段一 O-1 D_stage 中性记录，**不进入**阶段一 O-2
D → D_stage 完整性审计的 differs / identical / D-only / D_stage-only 分类。

**F-only 3 文件保留记录（仅追溯 D 端归属，不预判阶段三决策，**v8 per codex v7 P1 #1 整体移到阶段三 O-4**）**：

| F-only 文件 | P5 原始归属 | 阶段一处置（v8 per codex v7 P1 #1 整体移到阶段三 O-4） | 阶段三输入占位（v8 主落点） |
|------------|-------------|---------------------|---------------|
| gpu/fant_stackprotector.c | BC-22a（P5 line 491） | **excluded-deferred**（**不计入阶段一统计量**；v5 "applied" 处置已删除） | 待阶段三 O-4 + O-3 启动后裁决（`absorb` / `adapt` / `rewrite` / `base-retain` / `no-030`） |
| gpu/fant_stackprotector.h | BC-22a（P5 line 491） | **excluded-deferred** | 同上 |
| srvkm/include/common_ri_bridge.h | BC-22b（P5 line 492） | **excluded-deferred** | 同上 |

**严禁**：

- 在阶段一 O-1 中写入 `applied` 处置（v5 已删除）；
- 在阶段一 O-2 D → D_stage 完整性审计的 differs / identical / D-only /
  D_stage-only 分类中包含 F-only（违反 F0 隔离）；
- 在阶段一统计量（中性记录数 / D→D_stage diff 数）中计入 F-only；
- 在阶段三启动前预判 F-only 的 `base-retain` / `no-030` 决策。

## 五、O-1 闭合判定（v8；何时算"完成"）

O-1 视为**全闭合**（阶段一即可进入终审）**当且仅当**：

1. 19 个 Patch 行的 `追溯 BC` 字段全部填实（从 `per-file-classification.tsv` +
   23 BC 矩阵反查得到，**非估算**）；
2. 19 个 Patch 行的 `追溯 per-file 证据` 字段全部填实（依赖 O-2 闭合 +
   D → D_stage 完整性审计，详 `030-d-stage-audit.md` §四）；
3. 19 个 Patch 行的 `阶段一 终判` 字段从 TBD 改为 5 个 Deepin 端处置之一
   （`applied` / `duplicate` / `no-op` / `pure-rename` / `invalid`），**不包含**
   任何 F0 决策；
4. 19 个 Patch 行的 `阶段三 F0 决策` 字段**保持 TBD**（推迟到阶段三 O-3 +
   O-4 闭合；**严禁**在阶段一预填任何 F0 类目）；
5. 19 个 Patch 行的 `来源 patch` 字段填实（000-029 patch 编号 + SHA-256）；
6. 19 个 Patch 行的 `顺序` 字段填实（应用顺序约束）；
7. **F0 比较 / 030-NNN 字段全部留空**（推迟到阶段三）；
8. 中性变更记录**可重放得到当前 D_stage 源树**（逐字段回放测试通过）；
9. `双向追溯校验` 字段全部由"未闭合"改为"已闭合"（每 Patch 可追溯到
   1+ per-file 证据；每 per-file 证据可追溯到 1+ Patch）；
10. F-only 3 文件按 §四 阶段三输入占位规范登记（`excluded-deferred`，
    **不计入**阶段一统计量）；
11. 通过 codex 阶段一复跑 + finding 闭环。

**当前状态（v12 + 2026-09-07 阶段一开工）**：

- 第 1 项已闭合（2026-09-07 追溯 BC 反查填实：§三 19 行 + §三.1 明细 +
  §三.2 未匹配参照登记，全部来自 per-file-classification.tsv 原文反查，
  非估算）；
- 第 2 项未闭合（O-2 依赖 O-1 闭合前启动）；
- 第 3 项未闭合（处置待 O-2 differs 证据终判，批次3）；
- 第 5 项部分填实（§三.3：来源 patch 编号 19/19 填实；SHA-256 13/19
  有记录值待 sha256sum 复核 + 5 项 suspend 待实测 + stage-000 工具
  SHA，批次1b 需 Bash）；
- 第 6 项已填实（§三.3 顺序约束：patched-27 启用集 + suspend 组 024
  先行 + 关闭项不应用 + stage-000 基线最先）；
- 第 7 项已落实（v5 已删除 F0 / 030-NNN 字段，v6 三列分离强化，**v8**
  F-only 整体移到阶段三 O-4 per codex v7 P1 #1；v9-v12 保持该政策，仅
  配套标签同步）；
- 第 8-9 项未闭合；
- 第 10 项已落实（v6 §四 F-only excluded-deferred 登记；**v8** 明确为
  阶段三 O-4 输入占位；v9-v12 保持）；
- 第 11 项待启动。
- **O-1 v12 远未全闭合**，阶段一启动后才能推进。

## 六、不在 O-1 范围内

- 030-NNN 的最终类目（absorb / adapt / rewrite）写入（**推迟到阶段三**）；
- F0 文件 SHA-256 / F0 tree hash（**推迟到阶段三 O-4**）；
- F0 比较结果四分类（已覆盖 / 实现更好 / 实现不同 / 缺失）（**推迟到阶段三**）；
- 阶段三验证 fixture 的具体登记（依赖阶段二 O-3 闭合 + 阶段三 O-3 启动）；
- 任何 030-NNN patch 内容描述（属 `030-NNN.patch` 设计稿，**阶段三产物**）；
- F-only 3 文件的阶段一 `applied` 处置（违反 F0 隔离，v5 已删除；详 §四）。

## 七、配套记录（v12）

- `docs/planning/030-patch-rederivation-design.md`（v13 框架本体，三阶段方案 + 分阶段门槛 + tar.zst 可复现规范详 §五.3 + 显式 Git tag 流程详 §五.4 + tar/zstd 精确版本锁定详 §5.3 约束表 + reconcile 4 字段校验详 §5.3 reconcile 命令 + 持久化事务目录 + 结构化 journal 协议详 §5.3 + `--reference-manifest` CLI 解析详 §5.3）
- `docs/planning/030-d-stage-audit.md`（O-2 v13：D → D_stage 完整性审计 +
  tree-manifest `SRC_ROOT` 显式 export + 9 类 symlink 互斥 + F0 完全隔离
  + 9 文件输出 = 8 稳定证据 + 1 运行时 genesis.json + tar.zst 可复现规范
  + validate-then-replace + TSV 字段编码 + manifest schema 显式两路径检查
  + 目录级 staging + reconcile-first + 持久化事务目录 + 结构化 journal
  协议 + 启动恢复 + `--reference-manifest` CLI 解析 + 退出码子命令作用域
  契约 + `realpath -m` canonicalize）
- `docs/planning/evidence/d-stage-audit.tsv` + `.sha256` + `.genesis.json` +
  `.symlink.tsv` + `.symlink.tsv.sha256` + `.D.manifest.tsv` +
  `.D.manifest.tsv.sha256` + `.D_stage.manifest.tsv` +
  `.D_stage.manifest.tsv.sha256`（O-2 输出 v13 共 9 文件 = 8 稳定证据 +
  1 运行时 genesis.json，待阶段一启动后生成）
- `docs/planning/evidence/4.0.2-i3/`（阶段二验证证据 + `d-stage-snapshot.tar.zst`
  + `.sha256`，**v8 per codex v7 P1 #7** tar.zst 可复现规范详 030-design §五.3；
  待阶段二启动后生成）
- `docs/planning/evidence/o-stage/`（阶段三验证证据，待阶段三启动后生成）
- `/tmp/r16-d-stage/`（**v13 唯一合法 D_stage 物化根**，系统 `$TMPDIR`，
  **不在仓库任何路径下**；详 030-design §〇.5 / §四.1 / §五.3；**不写入**
  `migration/supervised-source-tree/_r16-d-stage/`，v6 该路径已取消）
- `collab/R16-2026-09-03-基座更新迭代评估/{qoder-notes,report}.md`
  （三方协作记录）
- `docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json`（阶段二 release 元数据，
  v11 per codex v10 P1 #3 路径统一到 evidence dir；待阶段二启动后生成）