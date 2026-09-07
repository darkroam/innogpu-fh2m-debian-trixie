# 19 项台账 → Deepin 中性变更记录映射表（O-1 v12 · P5/阶段一/阶段三 三列分离）

> 创建日期：2026-09-05（v5: 2026-09-05 dsh 三阶段返工指令后；v6: 2026-09-05 codex v5 初审 P1 #2 + P2 #6 闭环后；v7: 2026-09-05 codex v6 初审 P1 #1 + P1 #2 + P2 #6 配套修订后；v8: 2026-09-05 codex v7 初审 8 findings 闭环后（F-only 整体后移阶段三 O-4 / O-2 配套修订 / 9 类 symlink 闭合 / tar.zst 可复现规范）；v9: 2026-09-05 codex v8 初审 6 P1 + 2 P2 闭环后配套标签同步（O-1 主体 v8 决策无变化）；v10: 2026-09-05 codex v9 初审 5 P1 + 3 P2 闭环后配套标签同步（O-1 主体 v9 决策无变化）；v11: 2026-09-05 codex v10 初审 4 P1 + 1 P2 闭环后配套标签同步（O-1 主体 v10 决策无变化）；**v12: 2026-09-05 codex v11 初审 2 P1 + 2 P2 闭环后配套标签同步（O-1 主体 v11 决策无变化；per codex v12 P2 #5 要求 v11 残痕统一为 v12，§五 当前状态 v8 → v12，§七 配套记录 v11 → v12）；v13-v24 未修改（codex v12 初审 4 P1 + 1 P2 / codex v13 初审 2 P1 + 2 P2 / codex v14 初审 2 P1 + 2 P2 / codex v15 初审 2 P1 + 1 P2 / codex v16 初审 2 P1 + 1 P2 / codex v17 初审 2 P1 + 2 P2 / codex v18 初审 2 P1 + 1 P2 / codex v19 初审 1 P1 + 1 P2 / codex v20 初审 3 P1 / codex v21 初审 1 P1 / codex v22 初审 1 P1 / codex v23 初审 1 P1 + 1 P2 全部集中在 O-2 snapshot 协议与审计文档，与 O-1 表本体无直接关系）**）
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
| 1 | 001 | 18（P5 line 353） | adapted-port 16 项（构建前提） | TBD · reverse-lookup：`per-file-classification.tsv` BC-01（130 文件，74 BEHAVIORAL）+ O-2 `d-stage-audit.tsv` D→D_stage differs 行反查 | TBD | 未闭合（O-2 待启动） | 候选 |
| 2 | 002 | 2（P5 line 354） | adapted-port 2 项（F 完全缺失） | TBD · reverse-lookup：O-2 differs 行 + BC-01 / BC-09 范围 | TBD | 未闭合 | 候选 |
| 3 | 006 | 4（P5 line 355，含 3 sem-eq + 1 partial + reconcile） | 整体 adapted-port + reconcile | TBD · reverse-lookup：O-2 differs 行 + BC-07 / BC-09 / BC-16 范围 | TBD | 未闭合 | 候选 |
| 4 | 007 | 1（P5 line 356） | adapted-port 1 项 | TBD · reverse-lookup：O-2 differs 行 + BC-01 范围 | TBD | 未闭合 | 候选 |
| 5 | 009 | 4（P5 line 357） | adapted-port 4 项 | TBD · reverse-lookup：O-2 differs 行 + BC-13 范围 | TBD | 未闭合 | 候选 |
| 6 | 023 | 5（P5 line 358，运行时性能影响） | adapted-port 5 项 | TBD · reverse-lookup：O-2 differs 行 + BC-09 / BC-13 / BC-14 范围；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |
| 7 | 025-dma | 2（P5 line 359，DMA resv usage R/W） | adapted-port 2 项 | TBD · reverse-lookup：O-2 differs 行 + BC-16 范围 | TBD | 未闭合 | 候选 |
| 8 | 026-vblank | 1（P5 line 360，inactive CRTC guard） | adapted-port 1 项 | TBD · reverse-lookup：O-2 differs 行 + BC-09 / BC-16 范围 | TBD | 未闭合 | 候选 |
| 9 | 027 | 4（P5 line 361，prime_import 类型混淆 + reconcile） | adapted-port 3 项 + reconcile | TBD · reverse-lookup：O-2 differs 行 + BC-07 / BC-16 范围 | TBD | 未闭合 | 候选 |
| 10 | 024 | 1（P5 line 362，DVFS 协调前提） | adapted-port 1 项 | TBD · reverse-lookup：O-2 differs 行 + BC-17 / BC-13 范围 | TBD | 未闭合 | 候选 |
| 11 | 026-lifecycle | 3（P5 line 363，DVFS suspend/resume） | adapted-port 3 项 | TBD · reverse-lookup：O-2 differs 行 + BC-13 / BC-17 范围；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |
| 12 | 028 | 4（P5 line 364，原子计数协调） | adapted-port 4 项 | TBD · reverse-lookup：O-2 differs 行 + BC-10 / BC-14 / BC-16 范围 | TBD | 未闭合 | 候选 |
| 13 | 029 | 2（P5 line 365，DDCCI 显式逻辑） | adapted-port 2 项 | TBD · reverse-lookup：O-2 differs 行 + BC-09 范围 | TBD | 未闭合 | 候选 |
| 14 | stage-000 | 1（P5 line 366，初始基线，**尚未执行**） | runtime-verify | TBD · reverse-lookup：O-2 differs 行 + 跨 BC 范围；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |
| 15 | 003 | 2（P5 line 367，P3c 子表 + line 124） | retain（关闭项） | TBD · reverse-lookup：O-2 differs 行 + BC-01..BC-09 范围（候选 `retain` / `no-op` 之一；**不预设**） | TBD | 未闭合 | 候选 |
| 16 | 004 | 2（P5 line 368，P3c 子表 + line 124） | retain（关闭项） | TBD · reverse-lookup：O-2 differs 行 + BC-01..BC-09 范围（004 为 P3a 编号） | TBD | 未闭合 | 候选 |
| 17 | 005 | 1（P5 line 369，P3c 子表 + line 124） | retain（关闭项） | TBD · reverse-lookup：O-2 differs 行 + BC-01..BC-09 范围（005 为 P3a 编号） | TBD | 未闭合 | 候选 |
| 18 | 008 | 1（P5 line 370，P3c 子表 + line 124） | retain（关闭项） | TBD · reverse-lookup：O-2 differs 行 + BC-01..BC-09 范围（008 为 P3a 编号） | TBD | 未闭合 | 候选 |
| 19 | 025-display | 1（P5 line 371，根因假设未证实/证伪） | runtime-verify | TBD · reverse-lookup：O-2 differs 行 + BC-09 范围；运行时部分按框架 §七.1 走 UNVERIFIED | TBD | 未闭合 | 候选 |

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

**当前状态（v12）**：

- 第 1 项未闭合（BC 反查未跑）；
- 第 2 项未闭合（O-2 依赖 O-1 闭合前启动）；
- 第 3-6 项仅结构性列出（处置 / 来源 patch / 顺序均为 TBD；**显式标注
  reverse-lookup 路径**，详 §三 表格）；
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